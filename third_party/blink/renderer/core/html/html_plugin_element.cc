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

#include "base/feature_list.h"
#include "base/ranges/algorithm.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/fetch/fetch_api_request.mojom-blink.h"
#include "third_party/blink/public/mojom/loader/request_context_frame_type.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy.mojom-blink.h"
#include "third_party/blink/public/mojom/permissions_policy/policy_value.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/bindings/core/v8/script_controller.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/display_lock/display_lock_utilities.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/dom/node.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
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
#include "third_party/blink/renderer/core/page/plugin_data.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_request.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_from_url.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/scheduler/public/frame_scheduler.h"
#include "third_party/blink/renderer/platform/scheduler/public/scheduling_policy.h"

namespace blink {

namespace {

String GetMIMETypeFromURL(const KURL& url) {
  String filename = url.LastPathComponent().ToString();
  int extension_pos = filename.ReverseFind('.');
  if (extension_pos >= 0) {
    String extension = filename.Substring(extension_pos + 1);
    return MIMETypeRegistry::GetWellKnownMIMETypeForExtension(extension);
  }
  return String();
}

String ResolveMIMEType(const String& specified_type, const KURL& url) {
  if (!specified_type.empty()) {
    return specified_type;
  }
  // Try to guess the MIME type based off the extension.
  return GetMIMETypeFromURL(url);
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

void PluginParameters::MapDataParamToSrc() {
  if (base::ranges::any_of(names_, [](auto name) {
        return EqualIgnoringASCIICase(name, "src");
      })) {
    return;
  }

  auto data = base::ranges::find_if(
      names_, [](auto name) { return EqualIgnoringASCIICase(name, "data"); });

  if (data != names_.end()) {
    AppendNameWithValue(
        "src", values_[base::checked_cast<wtf_size_t>(data - names_.begin())]);
  }
}

HTMLPlugInElement::HTMLPlugInElement(const QualifiedName& tag_name,
                                     Document& doc,
                                     const CreateElementFlags flags)
    : HTMLFrameOwnerElement(tag_name, doc),
      ActiveScriptWrappable<HTMLPlugInElement>({}),
      is_delaying_load_event_(false),
      // needs_plugin_update_(!IsCreatedByParser) allows HTMLObjectElement to
      // delay EmbeddedContentView updates until after all children are
      // parsed. For HTMLEmbedElement this delay is unnecessary, but it is
      // simpler to make both classes share the same codepath in this class.
      needs_plugin_update_(!flags.IsCreatedByParser()) {
  SetHasCustomStyleCallbacks();
}

HTMLPlugInElement::~HTMLPlugInElement() {
  DCHECK(plugin_wrapper_.IsEmpty());  // cleared in detachLayoutTree()
  DCHECK(!is_delaying_load_event_);
}

void HTMLPlugInElement::Trace(Visitor* visitor) const {
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

void HTMLPlugInElement::SetFocused(bool focused,
                                   mojom::blink::FocusType focus_type) {
  WebPluginContainerImpl* plugin = OwnedPlugin();
  if (plugin)
    plugin->SetFocused(focused, focus_type);
  HTMLFrameOwnerElement::SetFocused(focused, focus_type);
}

bool HTMLPlugInElement::CanProcessDrag() const {
  // Be careful to call PluginEmbeddedContentView only once, because calling
  // it can change things such that another call will return a different
  // result.
  WebPluginContainerImpl* plugin = PluginEmbeddedContentView();
  return plugin && plugin->CanProcessDrag();
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

  LayoutObject* layout_object = GetLayoutObject();
  if (!layout_object || UseFallbackContent()) {
    // If we don't have a layoutObject we have to dispose of any plugins
    // which we persisted over a reattach.
    if (persisted_plugin_) {
      HTMLFrameOwnerElement::PluginDisposeSuspendScope suspend_plugin_dispose;
      SetPersistedPlugin(nullptr);
    }
    return;
  }

  // This element may have been attached previously, and if we created a frame
  // back then, re-use it now. We do not want to reload the frame if we don't
  // have to, as that would cause us to lose any state changed after loading.
  // Re-using the frame also matters if we have to re-attach for printing; we
  // don't support reloading anything during printing (the frame would just show
  // up blank then).
  const Frame* content_frame = ContentFrame();
  if (content_frame && !dispose_view_) {
    // We should only re-use the frame if we're actually re-attaching as
    // LayoutEmbeddedContent. We may for instance have become an image, without
    // having triggered a plugin reload, and that this layout object type change
    // happens now "for free" for completely different reasons (e.g. CSS display
    // type change).
    if (layout_object->IsLayoutEmbeddedContent())
      SetEmbeddedContentView(content_frame->View());
  } else if (!IsImageType() && NeedsPluginUpdate() &&
             GetLayoutEmbeddedObject() &&
             !GetLayoutEmbeddedObject()->ShowsUnavailablePluginIndicator() &&
             GetObjectContentType() != ObjectContentType::kPlugin &&
             !is_delaying_load_event_) {
    // If we're in a content-visibility subtree that can prevent layout, then
    // add our layout object to the frame view's update list. This is typically
    // done during layout, but if we're blocking layout, we will never update
    // the plugin and thus delay the load event indefinitely.
    if (DisplayLockUtilities::LockedAncestorPreventingLayout(*this)) {
      auto* embedded_object = GetLayoutEmbeddedObject();
      if (auto* frame_view = embedded_object->GetFrameView())
        frame_view->AddPartToUpdate(*embedded_object);
    }
    is_delaying_load_event_ = true;
    GetDocument().IncrementLoadEventDelayCount();
    GetDocument().LoadPluginsSoon();
  }
  if (image_loader_ && IsA<LayoutImage>(*layout_object)) {
    image_loader_->OnAttachLayoutTree();
  }
  if (layout_object->AffectsWhitespaceSiblings())
    context.previous_in_flow = layout_object;

  dispose_view_ = false;
}

void HTMLPlugInElement::IntrinsicSizingInfoChanged() {
  if (auto* embedded_object = GetLayoutEmbeddedObject())
    embedded_object->IntrinsicSizeChanged();
}

void HTMLPlugInElement::UpdatePlugin() {
  UpdatePluginInternal();
  if (is_delaying_load_event_) {
    is_delaying_load_event_ = false;
    GetDocument().DecrementLoadEventDelayCount();
  }
}

Node::InsertionNotificationRequest HTMLPlugInElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected())
    GetDocument().DelayLoadEventUntilLayoutTreeUpdate();
  return HTMLFrameOwnerElement::InsertedInto(insertion_point);
}

void HTMLPlugInElement::RemovedFrom(ContainerNode& insertion_point) {
  // Plugins can persist only through reattachment during a lifecycle
  // update. This method shouldn't be called in that lifecycle phase.
  DCHECK(!persisted_plugin_);

  HTMLFrameOwnerElement::RemovedFrom(insertion_point);
}

bool HTMLPlugInElement::ShouldAccelerate() const {
  WebPluginContainerImpl* plugin = OwnedPlugin();
  return plugin && plugin->CcLayer();
}

ParsedPermissionsPolicy HTMLPlugInElement::ConstructContainerPolicy() const {
  return GetLegacyFramePolicies();
}

void HTMLPlugInElement::DetachLayoutTree(bool performing_reattach) {
  // Update the EmbeddedContentView the next time we attach (detaching destroys
  // the plugin).
  // FIXME: None of this "needsPluginUpdate" related code looks right.
  if (GetLayoutObject() && !UseFallbackContent())
    SetNeedsPluginUpdate(true);

  if (is_delaying_load_event_) {
    is_delaying_load_event_ = false;
    GetDocument().DecrementLoadEventDelayCount();
  }

  bool keep_plugin = performing_reattach && !dispose_view_;

  // Only try to persist a plugin we actually own.
  WebPluginContainerImpl* plugin = OwnedPlugin();
  if (plugin && keep_plugin) {
    SetPersistedPlugin(
        To<WebPluginContainerImpl>(ReleaseEmbeddedContentView()));
  } else {
    // A persisted plugin isn't processed and hooked up immediately
    // (synchronously) when attaching the layout object, so it's possible that
    // it's still around. That's fine if we're allowed to keep it. Otherwise,
    // get rid of it now.
    if (persisted_plugin_ && !keep_plugin)
      SetPersistedPlugin(nullptr);

    // Clear the plugin; will trigger disposal of it with Oilpan.
    if (!persisted_plugin_)
      SetEmbeddedContentView(nullptr);
  }

  // We should attempt to use the same view afterwards, so that we don't lose
  // state. But only if we're reattaching. Otherwise we need to throw it away,
  // since there's no telling what's going to happen next, and it wouldn't be
  // safe to keep it.
  if (!performing_reattach)
    SetDisposeView();

  RemovePluginFromFrameView(plugin);
  ResetInstance();

  HTMLFrameOwnerElement::DetachLayoutTree(performing_reattach);
}

LayoutObject* HTMLPlugInElement::CreateLayoutObject(
    const ComputedStyle& style) {
  // Fallback content breaks the DOM->layoutObject class relationship of this
  // class and all superclasses because createObject won't necessarily return
  // a LayoutEmbeddedObject or LayoutEmbeddedContent.
  if (UseFallbackContent())
    return LayoutObject::CreateObject(this, style);

  if (IsImageType()) {
    LayoutImage* image = MakeGarbageCollected<LayoutImage>(this);
    image->SetImageResource(MakeGarbageCollected<LayoutImageResource>());
    return image;
  }

  plugin_is_available_ = true;
  return MakeGarbageCollected<LayoutEmbeddedObject>(this);
}

void HTMLPlugInElement::FinishParsingChildren() {
  HTMLFrameOwnerElement::FinishParsingChildren();
  if (!UseFallbackContent())
    SetNeedsPluginUpdate(true);
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
  v8::Isolate* isolate = GetDocument().GetAgent().isolate();
  if (plugin_wrapper_.IsEmpty()) {
    WebPluginContainerImpl* plugin;

    // Be careful to call PluginEmbeddedContentView only once, because calling
    // it can change things such that another call will return a different
    // result.
    if (persisted_plugin_)
      plugin = persisted_plugin_;
    else
      plugin = PluginEmbeddedContentView();

    if (plugin) {
      plugin_wrapper_.Reset(isolate, plugin->ScriptableObject(isolate));
    } else {
      // This step is intended for plugins with external handlers. This should
      // checked after after calling PluginEmbeddedContentView(). Note that
      // calling PluginEmbeddedContentView() leads to synchronously updating
      // style and running post layout tasks, which ends up updating the plugin.
      // It is after updating the plugin that we know whether or not the plugin
      // is handled externally. Also note that it is possible to call
      // PluginWrapper before the plugin has gone through the update phase(see
      // https://crbug.com/946709).
      if (!frame->Client())
        return v8::Local<v8::Object>();
      plugin_wrapper_.Reset(
          isolate, frame->Client()->GetScriptableObject(*this, isolate));
    }
  }
  return plugin_wrapper_.Get(isolate);
}

ScriptValue HTMLPlugInElement::AnonymousNamedGetter(const AtomicString& name) {
  if (!GetExecutionContext()) {
    // PluginWrapper() is guaranteed nullptr if there's no ExecutionContext.
    return ScriptValue();
  }

  v8::Isolate* isolate = GetExecutionContext()->GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  ScriptState* script_state = ScriptState::From(isolate, context);
  if (!script_state->World().IsMainWorld()) {
    if (script_state->World().IsIsolatedWorld()) {
      UseCounter::Count(GetExecutionContext(),
                        WebFeature::kPluginInstanceAccessFromIsolatedWorld);
    }
    // The plugin system cannot deal with multiple worlds, so block any
    // non-main world access.
    return ScriptValue();
  }
  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kPluginInstanceAccessFromMainWorld);

  v8::Local<v8::Object> instance = PluginWrapper();
  if (instance.IsEmpty()) {
    return ScriptValue();
  }

  v8::Local<v8::String> v8_name =
      V8AtomicString(script_state->GetIsolate(), name);
  bool has_own_property;
  v8::Local<v8::Value> value;
  if (!instance->HasOwnProperty(context, v8_name).To(&has_own_property) ||
      !has_own_property || !instance->Get(context, v8_name).ToLocal(&value)) {
    return ScriptValue();
  }

  UseCounter::Count(GetExecutionContext(),
                    WebFeature::kPluginInstanceAccessSuccessful);
  return ScriptValue(script_state->GetIsolate(), value);
}

NamedPropertySetterResult HTMLPlugInElement::AnonymousNamedSetter(
    const AtomicString& name,
    const ScriptValue& value) {
  if (!GetExecutionContext()) {
    // PluginWrapper() is guaranteed nullptr if there's no ExecutionContext.
    return NamedPropertySetterResult::kDidNotIntercept;
  }

  v8::Isolate* isolate = GetExecutionContext()->GetIsolate();
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  ScriptState* script_state = ScriptState::From(isolate, context);
  if (!script_state->World().IsMainWorld()) {
    // The plugin system cannot deal with multiple worlds, so block any
    // non-main world access.
    return NamedPropertySetterResult::kDidNotIntercept;
  }

  v8::Local<v8::Object> instance = PluginWrapper();
  if (instance.IsEmpty()) {
    return NamedPropertySetterResult::kDidNotIntercept;
  }

  // Don't intercept any of the properties of the HTMLPluginElement.
  v8::Local<v8::String> v8_name =
      V8AtomicString(script_state->GetIsolate(), name);
  v8::Local<v8::Object> this_wrapper =
      ToV8Traits<HTMLPlugInElement>::ToV8(script_state, this)
          .As<v8::Object>();
  bool instance_has_property;
  bool holder_has_property;
  if (!instance->HasOwnProperty(context, v8_name).To(&instance_has_property) ||
      !this_wrapper->Has(context, v8_name).To(&holder_has_property) ||
      (!instance_has_property && holder_has_property)) {
    return NamedPropertySetterResult::kDidNotIntercept;
  }

  // FIXME: The gTalk pepper plugin is the only plugin to make use of
  // SetProperty and that is being deprecated. This can be removed as soon as
  // it goes away.
  // Call SetProperty on a pepper plugin's scriptable object. Note that we
  // never set the return value here which would indicate that the plugin has
  // intercepted the SetProperty call, which means that the property on the
  // DOM element will also be set. For plugin's that don't intercept the call
  // (all except gTalk) this makes no difference at all. For gTalk the fact
  // that the property on the DOM element also gets set is inconsequential.
  bool created;
  if (!instance->CreateDataProperty(context, v8_name, value.V8Value())
           .To(&created)) {
    return NamedPropertySetterResult::kDidNotIntercept;
  }
  return NamedPropertySetterResult::kIntercepted;
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
    return To<WebPluginContainerImpl>(view);
  return nullptr;
}

bool HTMLPlugInElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr ||
      name == html_names::kVspaceAttr || name == html_names::kHspaceAttr ||
      name == html_names::kAlignAttr)
    return true;
  return HTMLFrameOwnerElement::IsPresentationAttribute(name);
}

void HTMLPlugInElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else if (name == html_names::kVspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginBottom, value);
  } else if (name == html_names::kHspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginRight, value);
  } else if (name == html_names::kAlignAttr) {
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
  if (auto* embedded_object = DynamicTo<LayoutEmbeddedObject>(r)) {
    if (embedded_object->ShowsUnavailablePluginIndicator())
      return;
  }
  if (WebPluginContainerImpl* plugin = OwnedPlugin()) {
    plugin->HandleEvent(event);
    if (event.DefaultHandled())
      return;
  }
  HTMLFrameOwnerElement::DefaultEventHandler(event);
}

LayoutEmbeddedContent* HTMLPlugInElement::LayoutEmbeddedContentForJSBindings()
    const {
  // Needs to load the plugin immediatedly because this function is called
  // when JavaScript code accesses the plugin.
  // FIXME: Check if dispatching events here is safe.
  GetDocument().UpdateStyleAndLayout(DocumentUpdateReason::kJavaScript);
  if (auto* view = GetDocument().View())
    view->FlushAnyPendingPostLayoutTasks();

  return ExistingLayoutEmbeddedContent();
}

bool HTMLPlugInElement::IsKeyboardFocusable(
    UpdateBehavior update_behavior) const {
  if (HTMLFrameOwnerElement::IsKeyboardFocusable(update_behavior)) {
    return true;
  }

  WebPluginContainerImpl* embedded_content_view = nullptr;
  if (LayoutEmbeddedContent* layout_embedded_content =
          ExistingLayoutEmbeddedContent()) {
    embedded_content_view = layout_embedded_content->Plugin();
  }

  return GetDocument().IsActive() && embedded_content_view &&
         embedded_content_view->SupportsKeyboardFocus() &&
         IsFocusable(update_behavior);
}

bool HTMLPlugInElement::HasCustomFocusLogic() const {
  return !UseFallbackContent();
}

bool HTMLPlugInElement::IsPluginElement() const {
  return true;
}

bool HTMLPlugInElement::IsErrorplaceholder() {
  // Be careful to call PluginEmbeddedContentView only once, because calling
  // it can change things such that another call will return a different
  // result.
  WebPluginContainerImpl* plugin = PluginEmbeddedContentView();
  return plugin && plugin->IsErrorplaceholder();
}

void HTMLPlugInElement::DisconnectContentFrame() {
  HTMLFrameOwnerElement::DisconnectContentFrame();
  SetPersistedPlugin(nullptr);
}

bool HTMLPlugInElement::IsFocusableStyle(UpdateBehavior update_behavior) const {
  if (HTMLFrameOwnerElement::SupportsFocus(update_behavior) !=
          FocusableState::kNotFocusable &&
      HTMLFrameOwnerElement::IsFocusableStyle(update_behavior)) {
    return true;
  }

  if (UseFallbackContent() ||
      !HTMLFrameOwnerElement::IsFocusableStyle(update_behavior)) {
    return false;
  }
  return plugin_is_available_;
}

HTMLPlugInElement::ObjectContentType HTMLPlugInElement::GetObjectContentType()
    const {
  KURL url = GetDocument().CompleteURL(url_);
  String mime_type = ResolveMIMEType(service_type_, url);
  if (mime_type.empty()) {
    return ObjectContentType::kFrame;
  }

  // If Chrome is started with the --disable-plugins switch, pluginData is 0.
  PluginData* plugin_data = GetDocument().GetFrame()->GetPluginData();
  bool plugin_supports_mime_type =
      plugin_data && plugin_data->SupportsMimeType(mime_type);
  if (plugin_supports_mime_type &&
      plugin_data->IsExternalPluginMimeType(mime_type)) {
    return ObjectContentType::kExternalPlugin;
  }

  if (MIMETypeRegistry::IsSupportedImageMIMEType(mime_type)) {
    return plugin_supports_mime_type ? ObjectContentType::kPlugin
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
  // HTMLObjectElement and HTMLEmbedElement may return arbitrary LayoutObjects
  // when using fallback content.
  return DynamicTo<LayoutEmbeddedObject>(GetLayoutObject());
}

// We don't use url_, as it may not be the final URL that the object loads,
// depending on <param> values.
bool HTMLPlugInElement::AllowedToLoadFrameURL(const String& url) {
  if (ContentFrame() && ProtocolIsJavaScript(url)) {
    return GetExecutionContext()->GetSecurityOrigin()->CanAccess(
        ContentFrame()->GetSecurityContext()->GetSecurityOrigin());
  }
  return true;
}

bool HTMLPlugInElement::RequestObject(const PluginParameters& plugin_params) {
  if (url_.empty() && service_type_.empty())
    return false;

  if (ProtocolIsJavaScript(url_))
    return false;

  KURL completed_url = url_.empty() ? KURL() : GetDocument().CompleteURL(url_);
  if (!AllowedToLoadObject(completed_url, service_type_))
    return false;

  ObjectContentType object_type = GetObjectContentType();
  bool handled_externally =
      object_type == ObjectContentType::kExternalPlugin &&
      AllowedToLoadPlugin(completed_url) &&
      GetDocument().GetFrame()->Client()->IsPluginHandledExternally(
          *this, completed_url, ResolveMIMEType(service_type_, completed_url));
  if (handled_externally)
    ResetInstance();
  if (object_type == ObjectContentType::kFrame ||
      object_type == ObjectContentType::kImage || handled_externally) {
    if (object_type == ObjectContentType::kFrame) {
      UseCounter::Count(GetDocument(),
                        WebFeature::kPluginElementLoadedDocument);
    } else if (object_type == ObjectContentType::kImage) {
      UseCounter::Count(GetDocument(), WebFeature::kPluginElementLoadedImage);
    } else {
      UseCounter::Count(GetDocument(),
                        WebFeature::kPluginElementLoadedExternal);
    }

    if (ContentFrame() && ContentFrame()->IsRemoteFrame()) {
      // During lazy reattaching, the plugin element loses EmbeddedContentView.
      // Since the ContentFrame() is not torn down the options here are to
      // either re-create a new RemoteFrameView or reuse the old one. The former
      // approach requires CommitNavigation for OOPF to be sent back here in
      // the parent process. It is easier to just reuse the current FrameView
      // instead until plugin element issue are properly resolved (for context
      // see https://crbug.com/781880).
      DCHECK(!OwnedEmbeddedContentView());
      SetEmbeddedContentView(ContentFrame()->View());
      DCHECK(OwnedEmbeddedContentView());
    }

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

bool HTMLPlugInElement::LoadPlugin(const KURL& url,
                                   const String& mime_type,
                                   const PluginParameters& plugin_params,
                                   bool use_fallback) {
  if (!AllowedToLoadPlugin(url)) {
    return false;
  }

  LocalFrame* frame = GetDocument().GetFrame();
  if (!frame->Loader().AllowPlugins())
    return false;

  auto* layout_object = GetLayoutEmbeddedObject();
  // FIXME: This code should not depend on layoutObject!
  if (!layout_object || use_fallback)
    return false;

  VLOG(1) << this << " Plugin URL: " << url_;
  VLOG(1) << "Loaded URL: " << url.GetString();
  loaded_url_ = url;

  if (persisted_plugin_) {
    auto* plugin = persisted_plugin_.Get();
    SetEmbeddedContentView(persisted_plugin_.Release());
    layout_object->GetFrameView()->AddPlugin(plugin);
  } else {
    bool load_manually =
        IsA<PluginDocument>(GetDocument()) && !GetDocument().ContainsPlugins();
    WebPluginContainerImpl* plugin = frame->Client()->CreatePlugin(
        *this, url, plugin_params.Names(), plugin_params.Values(), mime_type,
        load_manually);
    if (!plugin) {
      layout_object = GetLayoutEmbeddedObject();
      // LayoutObject can be destroyed between the previous check and here.
      if (layout_object && !layout_object->ShowsUnavailablePluginIndicator()) {
        plugin_is_available_ = false;
        layout_object->SetPluginAvailability(
            LayoutEmbeddedObject::kPluginMissing);
      }
      return false;
    }

    SetEmbeddedContentView(plugin);
    layout_object->GetFrameView()->AddPlugin(plugin);
  }

  // Disable back/forward cache when a document uses a plugin. This is not
  // done in the constructor since `HTMLPlugInElement` is a base class for
  // HTMLObjectElement and HTMLEmbedElement which can host child browsing
  // contexts instead.
  GetExecutionContext()->GetScheduler()->RegisterStickyFeature(
      SchedulingPolicy::Feature::kContainsPlugins,
      {SchedulingPolicy::DisableBackForwardCache()});

  GetDocument().SetContainsPlugins();
  // TODO(esprehn): WebPluginContainerImpl::SetCcLayer() also schedules a
  // compositing update, do we need both?
  SetNeedsCompositingUpdate();
  if (layout_object->HasLayer())
    layout_object->Layer()->SetNeedsCompositingInputsUpdate();
  return true;
}

void HTMLPlugInElement::DispatchErrorEvent() {
  ReportFallbackResourceTimingIfNeeded();
  if (IsA<PluginDocument>(GetDocument()) && GetDocument().LocalOwner()) {
    GetDocument().LocalOwner()->DispatchEvent(
        *Event::Create(event_type_names::kError));
  } else {
    DispatchEvent(*Event::Create(event_type_names::kError));
  }
}

bool HTMLPlugInElement::AllowedToLoadObject(const KURL& url,
                                            const String& mime_type) {
  if (url.IsEmpty() && mime_type.empty())
    return false;

  LocalFrame* frame = GetDocument().GetFrame();
  Settings* settings = frame->GetSettings();
  if (!settings)
    return false;

  if (MIMETypeRegistry::IsJavaAppletMIMEType(mime_type))
    return false;

  auto* csp = GetExecutionContext()->GetContentSecurityPolicy();
  if (!csp->AllowObjectFromSource(url)) {
    if (auto* layout_object = GetLayoutEmbeddedObject()) {
      plugin_is_available_ = false;
      layout_object->SetPluginAvailability(
          LayoutEmbeddedObject::kPluginBlockedByContentSecurityPolicy);
    }
    return false;
  }
  // If the URL is empty, a plugin could still be instantiated if a MIME-type
  // is specified.
  return (!mime_type.empty() && url.IsEmpty()) ||
         !MixedContentChecker::ShouldBlockFetch(
             frame, mojom::blink::RequestContextType::OBJECT,
             network::mojom::blink::IPAddressSpace::kUnknown, url,
             ResourceRequest::RedirectStatus::kNoRedirect, url,
             /* devtools_id= */ String(), ReportingDisposition::kReport,
             GetDocument().Loader()->GetContentSecurityNotifier());
}

bool HTMLPlugInElement::AllowedToLoadPlugin(const KURL& url) {
  if (GetExecutionContext()->IsSandboxed(
          network::mojom::blink::WebSandboxFlags::kPlugins)) {
    GetExecutionContext()->AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kSecurity,
            mojom::blink::ConsoleMessageLevel::kError,
            "Failed to load '" + url.ElidedString() +
                "' as a plugin, because the "
                "frame into which the plugin "
                "is loading is sandboxed."));
    return false;
  }
  return true;
}

void HTMLPlugInElement::RemovePluginFromFrameView(
    WebPluginContainerImpl* plugin) {
  if (!plugin)
    return;

  auto* layout_object = GetLayoutEmbeddedObject();
  if (!layout_object)
    return;

  auto* frame_view = layout_object->GetFrameView();
  if (!frame_view)
    return;

  if (!frame_view->Plugins().Contains(plugin))
    return;

  frame_view->RemovePlugin(plugin);
}

void HTMLPlugInElement::DidAddUserAgentShadowRoot(ShadowRoot&) {
  ShadowRoot* shadow_root = UserAgentShadowRoot();
  DCHECK(shadow_root);
  shadow_root->AppendChild(
      MakeGarbageCollected<HTMLSlotElement>(GetDocument()));
}

bool HTMLPlugInElement::HasFallbackContent() const {
  return false;
}

bool HTMLPlugInElement::UseFallbackContent() const {
  return false;
}

void HTMLPlugInElement::ReattachOnPluginChangeIfNeeded() {
  if (UseFallbackContent() || !NeedsPluginUpdate() || !GetLayoutObject())
    return;

  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(style_change_reason::kPluginChanged));
  SetForceReattachLayoutTree();

  // Make sure that we don't attempt to re-use the view through re-attachment.
  SetDisposeView();
}

void HTMLPlugInElement::UpdateServiceTypeIfEmpty() {
  if (service_type_.empty() && ProtocolIs(url_, "data")) {
    service_type_ = MimeTypeFromDataURL(url_);
  }
}

const ComputedStyle* HTMLPlugInElement::CustomStyleForLayoutObject(
    const StyleRecalcContext& style_recalc_context) {
  const ComputedStyle* style =
      OriginalStyleForLayoutObject(style_recalc_context);
  if (IsImageType() && !GetLayoutObject() && style &&
      LayoutObjectIsNeeded(*style)) {
    if (!image_loader_) {
      image_loader_ = MakeGarbageCollected<HTMLImageLoader>(this);
    }
    image_loader_->UpdateFromElement(ImageLoader::kUpdateNormal,
                                     /* force_blocking */ true);
  }
  return style;
}

}  // namespace blink
