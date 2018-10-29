/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "third_party/blink/renderer/core/html/html_object_element.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/dom/tag_collection.h"
#include "third_party/blink/renderer/core/dom/text.h"
#include "third_party/blink/renderer/core/exported/web_plugin_container_impl.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_param_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

using namespace HTMLNames;

inline HTMLObjectElement::HTMLObjectElement(Document& document,
                                            const CreateElementFlags flags)
    : HTMLPlugInElement(objectTag,
                        document,
                        flags,
                        kShouldNotPreferPlugInsForImages),
      use_fallback_content_(false) {}

inline HTMLObjectElement::~HTMLObjectElement() = default;

HTMLObjectElement* HTMLObjectElement::Create(Document& document,
                                             const CreateElementFlags flags) {
  auto* element = new HTMLObjectElement(document, flags);
  element->EnsureUserAgentShadowRoot();
  return element;
}

void HTMLObjectElement::Trace(blink::Visitor* visitor) {
  ListedElement::Trace(visitor);
  HTMLPlugInElement::Trace(visitor);
}

const HashSet<AtomicString>& HTMLObjectElement::GetCheckedAttributeNames()
    const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, attribute_set,
                      ({"data", "codebase"}));
  return attribute_set;
}

LayoutEmbeddedContent* HTMLObjectElement::ExistingLayoutEmbeddedContent()
    const {
  // This will return 0 if the layoutObject is not a LayoutEmbeddedContent.
  return GetLayoutEmbeddedContent();
}

bool HTMLObjectElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == borderAttr)
    return true;
  return HTMLPlugInElement::IsPresentationAttribute(name);
}

void HTMLObjectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == borderAttr)
    ApplyBorderAttributeToStyle(value, style);
  else
    HTMLPlugInElement::CollectStyleForPresentationAttribute(name, value, style);
}

void HTMLObjectElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == formAttr) {
    FormAttributeChanged();
  } else if (name == typeAttr) {
    SetServiceType(params.new_value.LowerASCII());
    wtf_size_t pos = service_type_.Find(";");
    if (pos != kNotFound)
      SetServiceType(service_type_.Left(pos));
    // TODO(schenney): crbug.com/572908 What is the right thing to do here?
    // Should we suppress the reload stuff when a persistable widget-type is
    // specified?
    ReloadPluginOnAttributeChange(name);
  } else if (name == dataAttr) {
    SetUrl(StripLeadingAndTrailingHTMLSpaces(params.new_value));
    if (GetLayoutObject() && IsImageType()) {
      SetNeedsPluginUpdate(true);
      if (!image_loader_)
        image_loader_ = HTMLImageLoader::Create(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else {
      ReloadPluginOnAttributeChange(name);
    }
  } else if (name == classidAttr) {
    class_id_ = params.new_value;
    ReloadPluginOnAttributeChange(name);
  } else {
    HTMLPlugInElement::ParseAttribute(params);
  }
}

static void MapDataParamToSrc(PluginParameters& plugin_params) {
  // Some plugins don't understand the "data" attribute of the OBJECT tag (i.e.
  // Real and WMP require "src" attribute).
  int src_index = plugin_params.FindStringInNames("src");
  int data_index = plugin_params.FindStringInNames("data");

  if (src_index == -1 && data_index != -1) {
    plugin_params.AppendNameWithValue("src",
                                      plugin_params.Values()[data_index]);
  }
}

// TODO(schenney): crbug.com/572908 This function should not deal with url or
// serviceType!
void HTMLObjectElement::ParametersForPlugin(PluginParameters& plugin_params) {
  HashSet<StringImpl*, CaseFoldingHash> unique_param_names;

  // Scan the PARAM children and store their name/value pairs.
  // Get the URL and type from the params if we don't already have them.
  for (HTMLParamElement* p = Traversal<HTMLParamElement>::FirstChild(*this); p;
       p = Traversal<HTMLParamElement>::NextSibling(*p)) {
    String name = p->GetName();
    if (name.IsEmpty())
      continue;

    unique_param_names.insert(name.Impl());
    plugin_params.AppendNameWithValue(p->GetName(), p->Value());

    // TODO(schenney): crbug.com/572908 url adjustment does not belong in this
    // function.
    // HTML5 says that an object resource's URL is specified by the object's
    // data attribute, not by a param element with a name of "data". However,
    // for compatibility, allow the resource's URL to be given by a param
    // element with one of the common names if we know that resource points
    // to a plugin.
    if (url_.IsEmpty() && !DeprecatedEqualIgnoringCase(name, "data") &&
        HTMLParamElement::IsURLParameter(name)) {
      SetUrl(StripLeadingAndTrailingHTMLSpaces(p->Value()));
    }
    // TODO(schenney): crbug.com/572908 serviceType calculation does not belong
    // in this function.
    if (service_type_.IsEmpty() && DeprecatedEqualIgnoringCase(name, "type")) {
      wtf_size_t pos = p->Value().Find(";");
      if (pos != kNotFound)
        SetServiceType(p->Value().GetString().Left(pos));
    }
  }

  // Turn the attributes of the <object> element into arrays, but don't override
  // <param> values.
  AttributeCollection attributes = Attributes();
  for (const Attribute& attribute : attributes) {
    const AtomicString& name = attribute.GetName().LocalName();
    if (!unique_param_names.Contains(name.Impl()))
      plugin_params.AppendAttribute(attribute);
  }

  MapDataParamToSrc(plugin_params);
}

bool HTMLObjectElement::HasFallbackContent() const {
  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    // Ignore whitespace-only text, and <param> tags, any other content is
    // fallback content.
    if (child->IsTextNode()) {
      if (!ToText(child)->ContainsOnlyWhitespaceOrEmpty())
        return true;
    } else if (!IsHTMLParamElement(*child)) {
      return true;
    }
  }
  return false;
}

bool HTMLObjectElement::HasValidClassId() const {
  if (MIMETypeRegistry::IsJavaAppletMIMEType(service_type_) &&
      ClassId().StartsWithIgnoringASCIICase("java:"))
    return true;

  // HTML5 says that fallback content should be rendered if a non-empty
  // classid is specified for which the UA can't find a suitable plugin.
  return ClassId().IsEmpty();
}

void HTMLObjectElement::ReloadPluginOnAttributeChange(
    const QualifiedName& name) {
  // Following,
  //   http://www.whatwg.org/specs/web-apps/current-work/#the-object-element
  //   (Enumerated list below "Whenever one of the following conditions occur:")
  //
  // the updating of certain attributes should bring about "redetermination"
  // of what the element contains.
  bool needs_invalidation;
  if (name == typeAttr) {
    needs_invalidation =
        !FastHasAttribute(classidAttr) && !FastHasAttribute(dataAttr);
  } else if (name == dataAttr) {
    needs_invalidation = !FastHasAttribute(classidAttr);
  } else if (name == classidAttr) {
    needs_invalidation = true;
  } else {
    NOTREACHED();
    needs_invalidation = false;
  }
  SetNeedsPluginUpdate(true);
  if (needs_invalidation)
    LazyReattachIfNeeded();
}

// TODO(schenney): crbug.com/572908 This should be unified with
// HTMLEmbedElement::UpdatePlugin and moved down into html_plugin_element.cc
void HTMLObjectElement::UpdatePluginInternal() {
  DCHECK(!GetLayoutEmbeddedObject()->ShowsUnavailablePluginIndicator());
  DCHECK(NeedsPluginUpdate());
  SetNeedsPluginUpdate(false);
  // TODO(schenney): crbug.com/572908 This should ASSERT
  // isFinishedParsingChildren() instead.
  if (!IsFinishedParsingChildren()) {
    DispatchErrorEvent();
    return;
  }

  // TODO(schenney): crbug.com/572908 I'm not sure it's ever possible to get
  // into updateWidget during a removal, but just in case we should avoid
  // loading the frame to prevent security bugs.
  if (!SubframeLoadingDisabler::CanLoadFrame(*this)) {
    DispatchErrorEvent();
    return;
  }

  PluginParameters plugin_params;
  ParametersForPlugin(plugin_params);

  // Note: url is modified above by parametersForPlugin.
  if (!AllowedToLoadFrameURL(url_)) {
    DispatchErrorEvent();
    return;
  }

  // TODO(schenney): crbug.com/572908 Is it possible to get here without a
  // layoutObject now that we don't have beforeload events?
  if (!GetLayoutObject())
    return;

  // Overwrites the URL and MIME type of a Flash embed to use an HTML5 embed.
  KURL overriden_url =
      GetDocument().GetFrame()->Client()->OverrideFlashEmbedWithHTML(
          GetDocument().CompleteURL(url_));
  if (!overriden_url.IsEmpty()) {
    url_ = overriden_url.GetString();
    SetServiceType("text/html");
  }

  if (!HasValidClassId() || !RequestObject(plugin_params)) {
    if (!url_.IsEmpty())
      DispatchErrorEvent();
    if (HasFallbackContent())
      RenderFallbackContent(ContentFrame());
  } else {
    if (IsErrorplaceholder())
      DispatchErrorEvent();
  }
}

Node::InsertionNotificationRequest HTMLObjectElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLPlugInElement::InsertedInto(insertion_point);
  ListedElement::InsertedInto(insertion_point);
  return kInsertionDone;
}

void HTMLObjectElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLPlugInElement::RemovedFrom(insertion_point);
  ListedElement::RemovedFrom(insertion_point);
}

void HTMLObjectElement::ChildrenChanged(const ChildrenChange& change) {
  if (isConnected() && !UseFallbackContent()) {
    SetNeedsPluginUpdate(true);
    LazyReattachIfNeeded();
  }
  HTMLPlugInElement::ChildrenChanged(change);
}

bool HTMLObjectElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == codebaseAttr ||
         attribute.GetName() == dataAttr ||
         (attribute.GetName() == usemapAttr && attribute.Value()[0] != '#') ||
         HTMLPlugInElement::IsURLAttribute(attribute);
}

bool HTMLObjectElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == classidAttr || name == dataAttr || name == codebaseAttr ||
         HTMLPlugInElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLObjectElement::SubResourceAttributeName() const {
  return dataAttr;
}

const AtomicString HTMLObjectElement::ImageSourceURL() const {
  return getAttribute(dataAttr);
}

void HTMLObjectElement::ReattachFallbackContent() {
  if (!GetDocument().InStyleRecalc())
    LazyReattachIfAttached();
}

void HTMLObjectElement::RenderFallbackContent(Frame* frame) {
  DCHECK(!frame || frame == ContentFrame());
  if (UseFallbackContent())
    return;

  if (!isConnected())
    return;

  // Before we give up and use fallback content, check to see if this is a MIME
  // type issue.
  if (image_loader_ && image_loader_->GetContent() &&
      image_loader_->GetContent()->GetContentStatus() !=
          ResourceStatus::kLoadError) {
    SetServiceType(image_loader_->GetContent()->GetResponse().MimeType());
    if (!IsImageType()) {
      // If we don't think we have an image type anymore, then clear the image
      // from the loader.
      image_loader_->ClearImage();
      ReattachFallbackContent();
      return;
    }
  }

  use_fallback_content_ = true;

  // TODO(schenney): crbug.com/572908 Style gets recalculated which is
  // suboptimal.
  ReattachFallbackContent();
}

bool HTMLObjectElement::IsExposed() const {
  // http://www.whatwg.org/specs/web-apps/current-work/#exposed
  for (HTMLObjectElement* ancestor =
           Traversal<HTMLObjectElement>::FirstAncestor(*this);
       ancestor;
       ancestor = Traversal<HTMLObjectElement>::FirstAncestor(*ancestor)) {
    if (ancestor->IsExposed())
      return false;
  }
  for (HTMLElement& element : Traversal<HTMLElement>::DescendantsOf(*this)) {
    if (IsHTMLObjectElement(element) || IsHTMLEmbedElement(element))
      return false;
  }
  return true;
}

bool HTMLObjectElement::ContainsJavaApplet() const {
  if (MIMETypeRegistry::IsJavaAppletMIMEType(getAttribute(typeAttr)))
    return true;

  for (HTMLElement& child : Traversal<HTMLElement>::ChildrenOf(*this)) {
    if (IsHTMLParamElement(child) &&
        DeprecatedEqualIgnoringCase(child.GetNameAttribute(), "type") &&
        MIMETypeRegistry::IsJavaAppletMIMEType(
            child.getAttribute(valueAttr).GetString()))
      return true;
    if (IsHTMLObjectElement(child) &&
        ToHTMLObjectElement(child).ContainsJavaApplet())
      return true;
  }

  return false;
}

void HTMLObjectElement::DidMoveToNewDocument(Document& old_document) {
  ListedElement::DidMoveToNewDocument(old_document);
  HTMLPlugInElement::DidMoveToNewDocument(old_document);
}

HTMLFormElement* HTMLObjectElement::formOwner() const {
  return ListedElement::Form();
}

bool HTMLObjectElement::IsInteractiveContent() const {
  return FastHasAttribute(usemapAttr);
}

bool HTMLObjectElement::UseFallbackContent() const {
  return HTMLPlugInElement::UseFallbackContent() || use_fallback_content_;
}

bool HTMLObjectElement::WillUseFallbackContentAtLayout() const {
  return !HasValidClassId() && HasFallbackContent();
}

void HTMLObjectElement::AssociateWith(HTMLFormElement* form) {
  AssociateByParser(form);
}

const HTMLObjectElement* ToHTMLObjectElementFromListedElement(
    const ListedElement* element) {
  SECURITY_DCHECK(!element || !element->IsFormControlElement());
  const HTMLObjectElement* object_element =
      static_cast<const HTMLObjectElement*>(element);
  // We need to assert after the cast because ListedElement doesn't
  // have hasTagName.
  SECURITY_DCHECK(!object_element ||
                  object_element->HasTagName(HTMLNames::objectTag));
  return object_element;
}

const HTMLObjectElement& ToHTMLObjectElementFromListedElement(
    const ListedElement& element) {
  return *ToHTMLObjectElementFromListedElement(&element);
}

}  // namespace blink
