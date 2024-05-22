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

#include "third_party/blink/renderer/core/css/style_change_reason.h"
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
#include "third_party/blink/renderer/core/html/html_embed_element.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_param_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"

namespace blink {

HTMLObjectElement::HTMLObjectElement(Document& document,
                                     const CreateElementFlags flags)
    : HTMLPlugInElement(html_names::kObjectTag, document, flags),
      use_fallback_content_(false) {
  EnsureUserAgentShadowRoot();
}

void HTMLObjectElement::Trace(Visitor* visitor) const {
  ListedElement::Trace(visitor);
  HTMLPlugInElement::Trace(visitor);
}

const AttrNameToTrustedType& HTMLObjectElement::GetCheckedAttributeTypes()
    const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({{"data", SpecificTrustedType::kScriptURL},
                        {"codebase", SpecificTrustedType::kScriptURL}}));
  return attribute_map;
}

LayoutEmbeddedContent* HTMLObjectElement::ExistingLayoutEmbeddedContent()
    const {
  // This will return 0 if the layoutObject is not a LayoutEmbeddedContent.
  return GetLayoutEmbeddedContent();
}

bool HTMLObjectElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kBorderAttr)
    return true;
  return HTMLPlugInElement::IsPresentationAttribute(name);
}

void HTMLObjectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kBorderAttr)
    ApplyBorderAttributeToStyle(value, style);
  else
    HTMLPlugInElement::CollectStyleForPresentationAttribute(name, value, style);
}

void HTMLObjectElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == html_names::kFormAttr) {
    FormAttributeChanged();
  } else if (name == html_names::kTypeAttr) {
    SetServiceType(params.new_value.LowerASCII());
    wtf_size_t pos = service_type_.Find(";");
    if (pos != kNotFound)
      SetServiceType(service_type_.Left(pos));
    // TODO(crbug.com/572908): What is the right thing to do here? Should we
    // suppress the reload stuff when a persistable widget-type is specified?
    ReloadPluginOnAttributeChange(name);
  } else if (name == html_names::kDataAttr) {
    SetUrl(StripLeadingAndTrailingHTMLSpaces(params.new_value));
    if (GetLayoutObject() && IsImageType()) {
      SetNeedsPluginUpdate(true);
      if (!image_loader_)
        image_loader_ = MakeGarbageCollected<HTMLImageLoader>(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else {
      ReloadPluginOnAttributeChange(name);
    }
  } else if (name == html_names::kClassidAttr) {
    class_id_ = params.new_value;
    ReloadPluginOnAttributeChange(name);
  } else {
    HTMLPlugInElement::ParseAttribute(params);
  }
}

void HTMLObjectElement::ParametersForPlugin(PluginParameters& plugin_params) {
  // Turn the attributes of the <object> element into arrays, but don't override
  // <param> values.
  for (const Attribute& attribute : Attributes()) {
    plugin_params.AppendAttribute(attribute);
  }

  // Some plugins don't understand the "data" attribute of the OBJECT tag (i.e.
  // Real and WMP require "src" attribute).
  plugin_params.MapDataParamToSrc();
}

bool HTMLObjectElement::HasFallbackContent() const {
  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    // Ignore whitespace-only text, and <param> tags, any other content is
    // fallback content.
    auto* child_text_node = DynamicTo<Text>(child);
    if (child_text_node) {
      if (!child_text_node->ContainsOnlyWhitespaceOrEmpty())
        return true;
    } else if (!IsA<HTMLParamElement>(*child)) {
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
  return ClassId().empty();
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
  if (name == html_names::kTypeAttr) {
    needs_invalidation = !FastHasAttribute(html_names::kClassidAttr) &&
                         !FastHasAttribute(html_names::kDataAttr);
  } else if (name == html_names::kDataAttr) {
    needs_invalidation = !FastHasAttribute(html_names::kClassidAttr);
  } else if (name == html_names::kClassidAttr) {
    needs_invalidation = true;
  } else {
    NOTREACHED_IN_MIGRATION();
    needs_invalidation = false;
  }
  SetNeedsPluginUpdate(true);
  if (needs_invalidation)
    ReattachOnPluginChangeIfNeeded();
}

// TODO(crbug.com/572908): This should be unified with
// HTMLEmbedElement::UpdatePlugin and moved down into html_plugin_element.cc
void HTMLObjectElement::UpdatePluginInternal() {
  DCHECK(!GetLayoutEmbeddedObject()->ShowsUnavailablePluginIndicator());
  DCHECK(NeedsPluginUpdate());
  SetNeedsPluginUpdate(false);
  // TODO(crbug.com/572908): This should ASSERT
  // isFinishedParsingChildren() instead.
  if (!IsFinishedParsingChildren()) {
    DispatchErrorEvent();
    return;
  }

  // TODO(crbug.com/572908): It may never be possible to get
  // into updateWidget during a removal, but just in case we should avoid
  // loading the frame to prevent security bugs.
  if (!SubframeLoadingDisabler::CanLoadFrame(*this)) {
    DispatchErrorEvent();
    return;
  }

  PluginParameters plugin_params;
  ParametersForPlugin(plugin_params);

  if (!AllowedToLoadFrameURL(url_)) {
    DispatchErrorEvent();
    return;
  }

  // TODO(crbug.com/572908): Is it possible to get here without a
  // layoutObject now that we don't have beforeload events?
  if (!GetLayoutObject())
    return;

  // Overwrites the URL and MIME type of a Flash embed to use an HTML5 embed.
  KURL overriden_url =
      GetDocument().GetFrame()->Client()->OverrideFlashEmbedWithHTML(
          GetDocument().CompleteURL(url_));
  if (!overriden_url.IsEmpty()) {
    UseCounter::Count(GetDocument(), WebFeature::kOverrideFlashEmbedwithHTML);
    url_ = overriden_url.GetString();
    SetServiceType("text/html");
  }

  if (!HasValidClassId() || !RequestObject(plugin_params)) {
    if (!url_.empty())
      DispatchErrorEvent();
    if (HasFallbackContent())
      RenderFallbackContent(ErrorEventPolicy::kDoNotDispatch);
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
  HTMLPlugInElement::ChildrenChanged(change);
  if (isConnected() && !UseFallbackContent()) {
    SetNeedsPluginUpdate(true);
    ReattachOnPluginChangeIfNeeded();
  }
}

bool HTMLObjectElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kCodebaseAttr ||
         attribute.GetName() == html_names::kDataAttr ||
         HTMLPlugInElement::IsURLAttribute(attribute);
}

bool HTMLObjectElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kClassidAttr || name == html_names::kDataAttr ||
         name == html_names::kCodebaseAttr ||
         HTMLPlugInElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLObjectElement::SubResourceAttributeName() const {
  return html_names::kDataAttr;
}

const AtomicString HTMLObjectElement::ImageSourceURL() const {
  return FastGetAttribute(html_names::kDataAttr);
}

void HTMLObjectElement::ReattachFallbackContent() {
  if (!GetDocument().InStyleRecalc()) {
    // TODO(futhark): Currently needs kSubtreeStyleChange because a style recalc
    // for the object element does not detect the changed need for descendant
    // style when we have a change in HTMLObjectElement::ChildrenCanHaveStyle().
    SetNeedsStyleRecalc(
        kSubtreeStyleChange,
        StyleChangeReasonForTracing::Create(style_change_reason::kUseFallback));
    SetForceReattachLayoutTree();
  }
}

void HTMLObjectElement::RenderFallbackContent(
    ErrorEventPolicy should_dispatch_error_event) {
  // This method approximately corresponds to step 7 from
  // https://whatwg.org/C/iframe-embed-object.html#the-object-element:
  //
  // If the load failed (e.g. there was an HTTP 404 error, there was a DNS
  // error), fire an event named error at the element, then jump to the step
  // below labeled fallback.
  if (should_dispatch_error_event == ErrorEventPolicy::kDispatch) {
    DispatchErrorEvent();
  }

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

  // TODO(dcheng): Detach the content frame here.
  UseCounter::Count(GetDocument(), WebFeature::kHTMLObjectElementFallback);
  use_fallback_content_ = true;
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
    if (IsA<HTMLObjectElement>(element) || IsA<HTMLEmbedElement>(element))
      return false;
  }
  return true;
}

bool HTMLObjectElement::ContainsJavaApplet() const {
  if (MIMETypeRegistry::IsJavaAppletMIMEType(
          FastGetAttribute(html_names::kTypeAttr)))
    return true;

  for (HTMLElement& child : Traversal<HTMLElement>::ChildrenOf(*this)) {
    if (IsA<HTMLParamElement>(child) &&
        EqualIgnoringASCIICase(child.GetNameAttribute(), "type") &&
        MIMETypeRegistry::IsJavaAppletMIMEType(
            child.FastGetAttribute(html_names::kValueAttr).GetString()))
      return true;

    auto* html_image_element = DynamicTo<HTMLObjectElement>(child);
    if (html_image_element && html_image_element->ContainsJavaApplet())
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

bool HTMLObjectElement::UseFallbackContent() const {
  return HTMLPlugInElement::UseFallbackContent() || use_fallback_content_;
}

bool HTMLObjectElement::WillUseFallbackContentAtLayout() const {
  return !HasValidClassId() && HasFallbackContent();
}

void HTMLObjectElement::AssociateWith(HTMLFormElement* form) {
  AssociateByParser(form);
}

bool HTMLObjectElement::DidFinishLoading() const {
  if (!isConnected())
    return false;
  if (OwnedPlugin())
    return true;
  if (auto* frame = ContentFrame()) {
    if (!frame->IsLoading())
      return true;
  }
  if (ImageLoader() && !HasPendingActivity() && IsImageType())
    return true;

  return UseFallbackContent();
}

int HTMLObjectElement::DefaultTabIndex() const {
  return 0;
}

const HTMLObjectElement* ToHTMLObjectElementFromListedElement(
    const ListedElement* element) {
  SECURITY_DCHECK(!element || !element->IsFormControlElement());
  const HTMLObjectElement* object_element =
      static_cast<const HTMLObjectElement*>(element);
  // We need to assert after the cast because ListedElement doesn't
  // have hasTagName.
  SECURITY_DCHECK(!object_element ||
                  object_element->HasTagName(html_names::kObjectTag));
  return object_element;
}

const HTMLObjectElement& ToHTMLObjectElementFromListedElement(
    const ListedElement& element) {
  return *ToHTMLObjectElementFromListedElement(&element);
}

}  // namespace blink
