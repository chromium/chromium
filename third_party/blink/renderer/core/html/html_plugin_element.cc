/**
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "third_party/blink/renderer/core/html/html_plugin_element.h"

#include "services/network/public/mojom/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/csp/content_security_policy.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_slot_element.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input/event_handler.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/loader/mixed_content_checker.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/page/scrolling/scrolling_coordinator.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_from_url.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/plugins/plugin_data.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Used for histograms, do not change the order.
enum PluginRequestObjectResult {
  kPluginRequestObjectResultFailure = 0,
  kPluginRequestObjectResultSuccess = 1,
  // Keep at the end.
  kPluginRequestObjectResultMax
};

String GetMIMETypeFromURL(const KURL& url) {
  String filename = url.LastPathComponent();
  int extension_pos = filename.ReverseFind('.');
  if (extension_pos >= 0) {
    String extension = filename.Substring(extension_pos + 1);
    return MIMETypeRegistry::GetWellKnownMIMETypeForExtension(extension);
  }
  return String();
}

}  // anonymous namespace

const Vector<String>& PluginParameters::Names() const {
  return names_;
}

const Vector<String>& PluginParameters::Values() const {
  return values_;
}

void PluginParameters::AppendAttribute(const Attribute& attribute) {
  names_.push_back(attribute.LocalName().GetString());
  values_.push_back(attribute.Value().GetString());
}
void PluginParameters::AppendNameWithValue(const String& name,
                                           const String& value) {
  names_.push_back(name);
  values_.push_back(value);
}

int PluginParameters::FindStringInNames(const String& str) {
  for (wtf_size_t i = 0; i < names_.size(); ++i) {
    if (DeprecatedEqualIgnoringCase(names_[i], str))
      return i;
  }
  return -1;
}

HTMLPlugInElement::HTMLPlugInElement(
    const QualifiedName& tag_name,
    Document& doc,
    const CreateElementFlags flags,
    PreferPlugInsForImagesOption prefer_plug_ins_for_images_option)
    : HTMLFrameOwnerElement(tag_name, doc),
      is_delaying_load_event_(false),
      // needs_plugin_update_(!IsCreatedByParser) allows HTMLObjectElement to
      // delay EmbeddedContentView updates until after all children are
      // parsed. For HTMLEmbedElement this delay is unnecessary, but it is
      // simpler to make both classes share the same codepath in this class.
      needs_plugin_update_(!flags.IsCreatedByParser()),
      should_prefer_plug_ins_for_images_(prefer_plug_ins_for_images_option ==
                                         kShouldPreferPlugInsForImages) {
  SetHasCustomStyleCallbacks();
}

HTMLPlugInElement::~HTMLPlugInElement() {
  DCHECK(plugin_wrapper_.IsEmpty());  // cleared in detachLayoutTree()
  DCHECK(!is_delaying_load_event_);
}

void HTMLPlugInElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_loader_);
  visitor->Trace(persisted_plugin_);
  HTMLFrameOwnerElement::Trace(visitor);
}

bool HTMLPlugInElement::HasPendingActivity() const {
  return image_loader_ && image_loader_->HasPendingActivity();
}

void HTMLPlugInElement::SetPersistedPlugin(WebPluginContainerImpl* plugin) {
  if (persisted_plugin_ == plugin)
    return;
  if (persisted_plugin_) {
    persisted_plugin_->Hide();
    DisposePluginSoon(persisted_plugin_.Release());
  }
  persisted_plugin_ = plugin;
}

void HTMLPlugInElement::SetFocused(bool focused, WebFocusType focus_type) {
  WebPluginContainerImpl* plugin = OwnedPlugin();
  if (plugin)
    plugin->SetFocused(focused, focus_type);
  HTMLFrameOwnerElement::SetFocused(focused, focus_type);
}

bool HTMLPlugInElement::RequestObjectInternal(
    const PluginParameters& plugin_params) {
  if (handled_externally_) {
    // TODO(ekaramad): Fix this once we know what to do with frames inside
    // plugins (https://crbug.com/776510).
    return true;
  }

  if (url_.IsEmpty() && service_type_.IsEmpty())
    return false;

  if (ProtocolIsJavaScript(url_))
    return false;

  KURL completed_url =
      url_.IsEmpty() ? KURL() : GetDocument().CompleteURL(url_);
  if (!AllowedToLoadObject(completed_url, service_type_))
    return false;

  handled_externally_ =
      GetDocument().GetFrame()->Client()->IsPluginHandledExternally(
          *this, completed_url,
          service_type_.IsEmpty() ? GetMIMETypeFromURL(completed_url)
                                  : service_type_);
  if (handled_externally_) {
    // This is a temporary placeholder and the logic around
    // |handled_externally_| might change as MimeHandlerView is moving towards
    // depending on OOPIFs instead of WebPlugin (https://crbug.com/659750).
    completed_url = BlankURL();
  }
  ObjectContentType object_type = GetObjectContentType();
  if (object_type == ObjectContentType::kFrame ||
      object_type == ObjectContentType::kImage || handled_externally_) {
    // If the plugin element already contains a subframe,
    // loadOrRedirectSubframe will re-use it. Otherwise, it will create a
    // new frame and set it as the LayoutEmbeddedContent's EmbeddedContentView,
    // causing what was previously in the EmbeddedContentView to be torn down.
    return LoadOrRedirectSubframe(completed_url, GetNameAttribute(), true);
  }

  // If an object's content can't be handled and it has no fallback, let
  // it be handled as a plugin to show the broken plugin icon.
  bool use_fallback =
      object_type == ObjectContentType::kNone && HasFallbackContent();
  return LoadPlugin(completed_url, service_type_, plugin_params, use_fallback);
}

bool HTMLPlugInElement::CanProcessDrag() const {
  return PluginEmbeddedContentView() &&
         PluginEmbeddedContentView()->CanProcessDrag();
}

bool HTMLPlugInElement::CanStartSelection() const {
  return UseFallbackContent() && Node::CanStartSelection();
}

bool HTMLPlugInElement::WillRespondToMouseClickEvents() {
  if (IsDisabledFormControl())
    return false;
  LayoutObject* r = GetLayoutObject();
  return r && (r->IsEmbeddedObject() || r->IsLayoutEmbeddedContent());
}

void HTMLPlugInElement::RemoveAllEventListeners() {
  HTMLFrameOwnerElement::RemoveAllEventListeners();
  WebPluginContainerImpl* plugin = OwnedPlugin();
  if (plugin)
    plugin->EventListenersRemoved();
}

void HTMLPlugInElement::DidMoveToNewDocument(Document& old_document) {
  if (image_loader_)
    image_loader_->ElementDidMoveToNewDocument();
  HTMLFrameOwnerElement::DidMoveToNewDocument(old_document);
}

void HTMLPlugInElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);

  if (!GetLayoutObject() || UseFallbackContent()) {
    // If we don't have a layoutObject we have to dispose of any plugins
    // which we persisted over a reattach.
    if (persisted_plugin_) {
      HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
      SetPersistedPlugin(nullptr);
    }
    return;
  }

  if (!IsImageType() && NeedsPluginUpdate() && GetLayoutEmbeddedObject() &&
      !GetLayoutEmbeddedObject()->ShowsUnavailablePluginIndicator() &&
      GetObjectContentType() != ObjectContentType::kPlugin &&
      !is_delaying_load_event_) {
    is_delaying_load_event_ = true;
    GetDocument().IncrementLoadEventDelayCount();
    GetDocument().LoadPluginsSoon();
  }
  if (LayoutObject* layout_object = GetLayoutObject()) {
    if (image_loader_ && layout_object->IsLayoutImage()) {
      LayoutImageResource* image_resource =
          ToLayoutImage(layout_object)->ImageResource();
      image_resource->SetImageResource(image_loader_->GetContent());
    }
    if (!layout_object->IsFloatingOrOutOfFlowPositioned())
      context.previous_in_flow = layout_object;
  }
}

void HTMLPlugInElement::IntrinsicSizingInfoChanged() {
  if (auto* layout_object = GetLayoutObject()) {
    layout_object->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
        LayoutInvalidationReason::kUnknown);
  }
}

void HTMLPlugInElement::UpdatePlugin() {
  UpdatePluginInternal();
  if (is_delaying_load_event_) {
    is_delaying_load_event_ = false;
    GetDocument().DecrementLoadEventDelayCount();
  }
}

void HTMLPlugInElement::RemovedFrom(ContainerNode& insertion_point) {
  // If we've persisted the plugin and we're removed from the tree then
  // make sure we cleanup the persistance pointer.
  if (persisted_plugin_) {
    // TODO(dcheng): This PluginDisposeSuspendScope doesn't seem to provide
    // much; investigate removing it.
    HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
    SetPersistedPlugin(nullptr);
  }
  HTMLFrameOwnerElement::RemovedFrom(insertion_point);
}

bool HTMLPlugInElement::ShouldAccelerate() const {
  WebPluginContainerImpl* plugin = OwnedPlugin();
  return plugin && plugin->CcLayer();
}

ParsedFeaturePolicy HTMLPlugInElement::ConstructContainerPolicy(
    Vector<String>*) const {
  // Plugin elements (<object> and <embed>) are not allowed to enable the
  // fullscreen feature. Add an empty whitelist for the fullscreen feature so
  // that the nested browsing context is unable to use the API, regardless of
  // origin.
  // https://fullscreen.spec.whatwg.org/#model
  ParsedFeaturePolicy container_policy;
  ParsedFeaturePolicyDeclaration whitelist;
  whitelist.feature = mojom::FeaturePolicyFeature::kFullscreen;
  whitelist.matches_all_origins = false;
  container_policy.push_back(whitelist);
  return container_policy;
}

void HTMLPlugInElement::DetachLayoutTree(const AttachContext& context) {
  // Update the EmbeddedContentView the next time we attach (detaching destroys
  // the plugin).
  // FIXME: None of this "needsPluginUpdate" related code looks right.
  if (GetLayoutObject() && !UseFallbackContent())
    SetNeedsPluginUpdate(true);

  if (is_delaying_load_event_) {
    is_delaying_load_event_ = false;
    GetDocument().DecrementLoadEventDelayCount();
  }

  // Only try to persist a plugin we actually own.
  WebPluginContainerImpl* plugin = OwnedPlugin();
  if (plugin && context.performing_reattach) {
    SetPersistedPlugin(ToWebPluginContainerImpl(ReleaseEmbeddedContentView()));
  } else {
    // Clear the plugin; will trigger disposal of it with Oilpan.
    SetEmbeddedContentView(nullptr);
  }

  ResetInstance();

  HTMLFrameOwnerElement::DetachLayoutTree(context);
}

LayoutObject* HTMLPlugInElement::CreateLayoutObject(
    const ComputedStyle& style) {
  // Fallback content breaks the DOM->layoutObject class relationship of this
  // class and all superclasses because createObject won't necessarily return
  // a LayoutEmbeddedObject or LayoutEmbeddedContent.
  if (UseFallbackContent())
    return LayoutObject::CreateObject(this, style);

  if (IsImageType()) {
    LayoutImage* image = new LayoutImage(this);
    image->SetImageResource(LayoutImageResource::Create());
    return image;
  }

  plugin_is_available_ = true;
  return new LayoutEmbeddedObject(this);
}

void HTMLPlugInElement::FinishParsingChildren() {
  HTMLFrameOwnerElement::FinishParsingChildren();
  if (UseFallbackContent())
    return;

  SetNeedsPluginUpdate(true);
  if (isConnected())
    LazyReattachIfNeeded();
}

void HTMLPlugInElement::ResetInstance() {
  plugin_wrapper_.Reset();
}

v8::Local<v8::Object> HTMLPlugInElement::PluginWrapper() {
  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame)
    return v8::Local<v8::Object>();

  // If the host dynamically turns off JavaScript (or Java) we will still
  // return the cached allocated Bindings::Instance. Not supporting this
  // edge-case is OK.
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  if (plugin_wrapper_.IsEmpty()) {
    WebPluginContainerImpl* plugin;

    if (persisted_plugin_)
      plugin = persisted_plugin_;
    else
      plugin = PluginEmbeddedContentView();

    if (plugin)
      plugin_wrapper_.Reset(isolate, plugin->ScriptableObject(isolate));
  }
  return plugin_wrapper_.Get(isolate);
}

WebPluginContainerImpl* HTMLPlugInElement::PluginEmbeddedContentView() const {
  if (LayoutEmbeddedContent* layout_embedded_content =
          LayoutEmbeddedContentForJSBindings())
    return layout_embedded_content->Plugin();
  return nullptr;
}

WebPluginContainerImpl* HTMLPlugInElement::OwnedPlugin() const {
  EmbeddedContentView* view = OwnedEmbeddedContentView();
  if (view && view->IsPluginView())
    return ToWebPluginContainerImpl(view);
  return nullptr;
}

bool HTMLPlugInElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == widthAttr || name == heightAttr || name == vspaceAttr ||
      name == hspaceAttr || name == alignAttr)
    return true;
  return HTMLFrameOwnerElement::IsPresentationAttribute(name);
}

void HTMLPlugInElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == widthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == vspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (name == hspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (name == alignAttr) {
    ApplyAlignmentAttributeToStyle(value, style);
  } else {
    HTMLFrameOwnerElement::CollectStyleForPresentationAttribute(name, value,
                                                                style);
  }
}

void HTMLPlugInElement::DefaultEventHandler(Event& event) {
  // Firefox seems to use a fake event listener to dispatch events to plugin
  // (tested with mouse events only). This is observable via different order
  // of events - in Firefox, event listeners specified in HTML attributes
  // fires first, then an event gets dispatched to plugin, and only then
  // other event listeners fire. Hopefully, this difference does not matter in
  // practice.

  // FIXME: Mouse down and scroll events are passed down to plugin via custom
  // code in EventHandler; these code paths should be united.

  LayoutObject* r = GetLayoutObject();
  if (!r || !r->IsLayoutEmbeddedContent())
    return;
  if (r->IsEmbeddedObject()) {
    if (ToLayoutEmbeddedObject(r)->ShowsUnavailablePluginIndicator())
      return;
  }
  WebPluginContainerImpl* plugin = OwnedPlugin();
  if (!plugin)
    return;
  plugin->HandleEvent(event);
  if (event.DefaultHandled())
    return;
  HTMLFrameOwnerElement::DefaultEventHandler(event);
}

LayoutEmbeddedContent* HTMLPlugInElement::LayoutEmbeddedContentForJSBindings()
    const {
  // Needs to load the plugin immediatedly because this function is called
  // when JavaScript code accesses the plugin.
  // FIXME: Check if dispatching events here is safe.
  GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets(
      Document::kRunPostLayoutTasksSynchronously);
  return ExistingLayoutEmbeddedContent();
}

bool HTMLPlugInElement::IsKeyboardFocusable() const {
  if (HTMLFrameOwnerElement::IsKeyboardFocusable())
    return true;
  return GetDocument().IsActive() && PluginEmbeddedContentView() &&
         PluginEmbeddedContentView()->SupportsKeyboardFocus();
}

bool HTMLPlugInElement::HasCustomFocusLogic() const {
  return !UseFallbackContent();
}

bool HTMLPlugInElement::IsPluginElement() const {
  return true;
}

bool HTMLPlugInElement::IsErrorplaceholder() {
  if (PluginEmbeddedContentView() &&
      PluginEmbeddedContentView()->IsErrorplaceholder())
    return true;
  return false;
}

void HTMLPlugInElement::DisconnectContentFrame() {
  HTMLFrameOwnerElement::DisconnectContentFrame();
  SetPersistedPlugin(nullptr);
}

bool HTMLPlugInElement::IsFocusableStyle() const {
  if (HTMLFrameOwnerElement::SupportsFocus() &&
      HTMLFrameOwnerElement::IsFocusableStyle())
    return true;

  if (UseFallbackContent() || !HTMLFrameOwnerElement::IsFocusableStyle())
    return false;
  return plugin_is_available_;
}

HTMLPlugInElement::ObjectContentType HTMLPlugInElement::GetObjectContentType()
    const {
  String mime_type = service_type_;
  KURL url = GetDocument().CompleteURL(url_);
  if (mime_type.IsEmpty()) {
    // Try to guess the MIME type based off the extension.
    mime_type = GetMIMETypeFromURL(url);
    if (mime_type.IsEmpty())
      return ObjectContentType::kFrame;
  }

  // If Chrome is started with the --disable-plugins switch, pluginData is 0.
  PluginData* plugin_data = GetDocument().GetFrame()->GetPluginData();
  bool plugin_supports_mime_type =
      plugin_data && plugin_data->SupportsMimeType(mime_type);

  if (MIMETypeRegistry::IsSupportedImageMIMEType(mime_type)) {
    return should_prefer_plug_ins_for_images_ && plugin_supports_mime_type
               ? ObjectContentType::kPlugin
               : ObjectContentType::kImage;
  }

  if (plugin_supports_mime_type)
    return ObjectContentType::kPlugin;
  if (MIMETypeRegistry::IsSupportedNonImageMIMEType(mime_type))
    return ObjectContentType::kFrame;
  return ObjectContentType::kNone;
}

bool HTMLPlugInElement::IsImageType() const {
  if (GetDocument().GetFrame())
    return GetObjectContentType() == ObjectContentType::kImage;
  return MIMETypeRegistry::IsSupportedImageResourceMIMEType(service_type_);
}

LayoutEmbeddedObject* HTMLPlugInElement::GetLayoutEmbeddedObject() const {
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary layoutObjects
  // when using fallback content.
  if (!GetLayoutObject() || !GetLayoutObject()->IsEmbeddedObject())
    return nullptr;
  return ToLayoutEmbeddedObject(GetLayoutObject());
}

// We don't use url_, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInElement::AllowedToLoadFrameURL(const String& url) {
  KURL complete_url = GetDocument().CompleteURL(url);
  return !(ContentFrame() && complete_url.ProtocolIsJavaScript() &&
           !GetDocument().GetSecurityOrigin()->CanAccess(
               ContentFrame()->GetSecurityContext()->GetSecurityOrigin()));
}

bool HTMLPlugInElement::RequestObject(const PluginParameters& plugin_params) {
  bool result = RequestObjectInternal(plugin_params);

  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, result_histogram,
      ("Plugin.RequestObjectResult", kPluginRequestObjectResultMax));
  result_histogram.Count(result ? kPluginRequestObjectResultSuccess
                                : kPluginRequestObjectResultFailure);

  return result;
}

bool HTMLPlugInElement::LoadPlugin(const KURL& url,
                                   const String& mime_type,
                                   const PluginParameters& plugin_params,
                                   bool use_fallback) {
  if (!AllowedToLoadPlugin(url, mime_type))
    return false;

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame->Loader().AllowPlugins(kAboutToInstantiatePlugin))
    return false;

  auto* layout_object = GetLayoutEmbeddedObject();
  // FIXME: This code should not depend on layoutObject!
  if (!layout_object || use_fallback)
    return false;

  VLOG(1) << this << " Plugin URL: " << url_;
  VLOG(1) << "Loaded URL: " << url.GetString();
  loaded_url_ = url;

  if (persisted_plugin_) {
    SetEmbeddedContentView(persisted_plugin_.Release());
  } else {
    bool load_manually =
        GetDocument().IsPluginDocument() && !GetDocument().ContainsPlugins();
    WebPluginContainerImpl* plugin = frame->Client()->CreatePlugin(
        *this, url, plugin_params.Names(), plugin_params.Values(), mime_type,
        load_manually);
    if (!plugin) {
      if (layout_object && !layout_object->ShowsUnavailablePluginIndicator()) {
        plugin_is_available_ = false;
        layout_object->SetPluginAvailability(
            LayoutEmbeddedObject::kPluginMissing);
      }
      return false;
    }

    if (layout_object) {
      SetEmbeddedContentView(plugin);
      layout_object->GetFrameView()->AddPlugin(plugin);
    } else {
      SetPersistedPlugin(plugin);
    }
  }

  GetDocument().SetContainsPlugins();
  // TODO(esprehn): WebPluginContainerImpl::SetCcLayer() also schedules a
  // compositing update, do we need both?
  SetNeedsCompositingUpdate();
  // Make sure any input event handlers introduced by the plugin are taken into
  // account.
  if (Page* page = GetDocument().GetFrame()->GetPage()) {
    if (ScrollingCoordinator* scrolling_coordinator =
            page->GetScrollingCoordinator()) {
      LocalFrameView* frame_view = GetDocument().GetFrame()->View();
      scrolling_coordinator->NotifyGeometryChanged(frame_view);
    }
  }
  return true;
}

void HTMLPlugInElement::DispatchErrorEvent() {
  if (GetDocument().IsPluginDocument() && GetDocument().LocalOwner()) {
    GetDocument().LocalOwner()->DispatchEvent(
        *Event::Create(EventTypeNames::error));
  } else {
    DispatchEvent(*Event::Create(EventTypeNames::error));
  }
}

bool HTMLPlugInElement::AllowedToLoadObject(const KURL& url,
                                            const String& mime_type) {
  if (url.IsEmpty() && mime_type.IsEmpty())
    return false;

  LocalFrame* frame = GetDocument().GetFrame();
  Settings* settings = frame->GetSettings();
  if (!settings)
    return false;

  if (MIMETypeRegistry::IsJavaAppletMIMEType(mime_type))
    return false;

  AtomicString declared_mime_type = FastGetAttribute(HTMLNames::typeAttr);
  if (!GetDocument().GetContentSecurityPolicy()->AllowObjectFromSource(url) ||
      !GetDocument().GetContentSecurityPolicy()->AllowPluginTypeForDocument(
          GetDocument(), mime_type, declared_mime_type, url)) {
    if (auto* layout_object = GetLayoutEmbeddedObject()) {
      plugin_is_available_ = false;
      layout_object->SetPluginAvailability(
          LayoutEmbeddedObject::kPluginBlockedByContentSecurityPolicy);
    }
    return false;
  }
  // If the URL is empty, a plugin could still be instantiated if a MIME-type
  // is specified.
  return (!mime_type.IsEmpty() && url.IsEmpty()) ||
         !MixedContentChecker::ShouldBlockFetch(
             frame, mojom::RequestContextType::OBJECT,
             network::mojom::RequestContextFrameType::kNone,
             ResourceRequest::RedirectStatus::kNoRedirect, url);
}

bool HTMLPlugInElement::AllowedToLoadPlugin(const KURL& url,
                                            const String& mime_type) {
  if (GetDocument().IsSandboxed(kSandboxPlugins)) {
    GetDocument().AddConsoleMessage(
        ConsoleMessage::Create(kSecurityMessageSource, kErrorMessageLevel,
                               "Failed to load '" + url.ElidedString() +
                                   "' as a plugin, because the "
                                   "frame into which the plugin "
                                   "is loading is sandboxed."));
    return false;
  }
  return true;
}

void HTMLPlugInElement::DidAddUserAgentShadowRoot(ShadowRoot&) {
  UserAgentShadowRoot()->AppendChild(
      HTMLSlotElement::CreateUserAgentDefaultSlot(GetDocument()));
}

bool HTMLPlugInElement::HasFallbackContent() const {
  return false;
}

bool HTMLPlugInElement::UseFallbackContent() const {
  return false;
}

void HTMLPlugInElement::LazyReattachIfNeeded() {
  if (!UseFallbackContent() && NeedsPluginUpdate() && GetLayoutObject() &&
      !IsImageType()) {
    LazyReattachIfAttached();
    SetPersistedPlugin(nullptr);
  }
}

void HTMLPlugInElement::UpdateServiceTypeIfEmpty() {
  if (service_type_.IsEmpty() && ProtocolIs(url_, "data")) {
    service_type_ = MimeTypeFromDataURL(url_);
  }
}

scoped_refptr<ComputedStyle> HTMLPlugInElement::CustomStyleForLayoutObject() {
  scoped_refptr<ComputedStyle> style = OriginalStyleForLayoutObject();
  if (IsImageType() && !GetLayoutObject() && style &&
      LayoutObjectIsNeeded(*style)) {
    if (!image_loader_)
      image_loader_ = HTMLImageLoader::Create(this);
    image_loader_->UpdateFromElement();
  }
  return style;
}

}  // namespace blink
