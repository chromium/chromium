/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2011 Apple Inc. All rights
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

#include "third_party/blink/renderer/core/html/html_embed_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/node_computed_style.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"

namespace blink {

HTMLEmbedElement::HTMLEmbedElement(Document& document,
                                   const CreateElementFlags flags)
    : HTMLPlugInElement(html_names::kEmbedTag, document, flags) {
  EnsureUserAgentShadowRoot();
}

const AttrNameToTrustedType& HTMLEmbedElement::GetCheckedAttributeTypes()
    const {
  DEFINE_STATIC_LOCAL(AttrNameToTrustedType, attribute_map,
                      ({{"src", SpecificTrustedType::kScriptURL}}));
  return attribute_map;
}

static inline LayoutEmbeddedContent* FindPartLayoutObject(const Node* n) {
  if (!n->GetLayoutObject())
    n = Traversal<HTMLObjectElement>::FirstAncestor(*n);

  if (n)
    return DynamicTo<LayoutEmbeddedContent>(n->GetLayoutObject());
  return nullptr;
}

LayoutEmbeddedContent* HTMLEmbedElement::ExistingLayoutEmbeddedContent() const {
  return FindPartLayoutObject(this);
}

bool HTMLEmbedElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kHiddenAttr)
    return true;
  return HTMLPlugInElement::IsPresentationAttribute(name);
}

void HTMLEmbedElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kHiddenAttr) {
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kWidth, 0, CSSPrimitiveValue::UnitType::kPixels);
    AddPropertyToPresentationAttributeStyle(
        style, CSSPropertyID::kHeight, 0, CSSPrimitiveValue::UnitType::kPixels);
  } else {
    HTMLPlugInElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLEmbedElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kTypeAttr) {
    SetServiceType(params.new_value.LowerASCII());
    wtf_size_t pos = service_type_.Find(";");
    if (pos != kNotFound)
      SetServiceType(service_type_.Left(pos));
    SetDisposeView();
    if (GetLayoutObject()) {
      SetNeedsPluginUpdate(true);
      GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
          "Embed type changed");
    }
  } else if (params.name == html_names::kCodeAttr) {
    // TODO(rendering-core): Remove this branch? It's not in the spec and we're
    // not in the HTMLAppletElement hierarchy.
    SetUrl(StripLeadingAndTrailingHTMLSpaces(params.new_value));
    SetDisposeView();
  } else if (params.name == html_names::kSrcAttr) {
    // https://html.spec.whatwg.org/multipage/iframe-embed-object.html#the-embed-element
    // The spec says that when the url attribute is changed and the embed
    // element is "potentially active," we should run the embed element setup
    // steps.
    // We don't follow the "potentially active" definition precisely here, but
    // it works.
    SetUrl(StripLeadingAndTrailingHTMLSpaces(params.new_value));
    SetDisposeView();
    if (GetLayoutObject() && IsImageType()) {
      if (!image_loader_)
        image_loader_ = MakeGarbageCollected<HTMLImageLoader>(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else if (GetLayoutObject()) {
      if (!FastHasAttribute(html_names::kTypeAttr)) {
        UseCounter::Count(GetDocument(),
                          WebFeature::kEmbedElementWithoutTypeSrcChanged);
      }
      SetNeedsPluginUpdate(true);
      ReattachOnPluginChangeIfNeeded();
    }
  } else {
    HTMLPlugInElement::ParseAttribute(params);
  }
}

void HTMLEmbedElement::ParametersForPlugin(PluginParameters& plugin_params) {
  AttributeCollection attributes = Attributes();
  for (const Attribute& attribute : attributes)
    plugin_params.AppendAttribute(attribute);
}

// FIXME: This should be unified with HTMLObjectElement::UpdatePlugin and
// moved down into html_plugin_element.cc
void HTMLEmbedElement::UpdatePluginInternal() {
  DCHECK(!GetLayoutEmbeddedObject()->ShowsUnavailablePluginIndicator());
  DCHECK(NeedsPluginUpdate());
  SetNeedsPluginUpdate(false);

  if (url_.empty() && service_type_.empty())
    return;

  // Note these pass url_ and service_type_ to allow better code sharing with
  // <object> which modifies url and serviceType before calling these.
  if (!AllowedToLoadFrameURL(url_))
    return;

  PluginParameters plugin_params;
  ParametersForPlugin(plugin_params);

  // FIXME: Can we not have GetLayoutObject() here now that beforeload events
  // are gone?
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

  RequestObject(plugin_params);
}

bool HTMLEmbedElement::LayoutObjectIsNeeded(const DisplayStyle& style) const {
  // In the current specification, there is no requirement for `ImageType` to
  // enforce layout.
  if (!RuntimeEnabledFeatures::HTMLEmbedElementNotForceLayoutEnabled() &&
      IsImageType()) {
    return HTMLPlugInElement::LayoutObjectIsNeeded(style);
  }

  // https://html.spec.whatwg.org/C/#the-embed-element
  // While any of the following conditions are occurring, any plugin
  // instantiated for the element must be removed, and the embed element
  // represents nothing:

  // * The element has neither a src attribute nor a type attribute.
  if (!FastHasAttribute(html_names::kSrcAttr) &&
      !FastHasAttribute(html_names::kTypeAttr))
    return false;

  // * The element has a media element ancestor.
  // -> It's realized by LayoutMedia::isChildAllowed.

  // * The element has an ancestor object element that is not showing its
  //   fallback content.
  ContainerNode* p = parentNode();
  if (auto* object = DynamicTo<HTMLObjectElement>(p)) {
    if (!object->WillUseFallbackContentAtLayout() &&
        !object->UseFallbackContent()) {
      return false;
    }
  }
  return HTMLPlugInElement::LayoutObjectIsNeeded(style);
}

bool HTMLEmbedElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr ||
         HTMLPlugInElement::IsURLAttribute(attribute);
}

const QualifiedName& HTMLEmbedElement::SubResourceAttributeName() const {
  return html_names::kSrcAttr;
}

bool HTMLEmbedElement::IsInteractiveContent() const {
  return true;
}

bool HTMLEmbedElement::IsExposed() const {
  // http://www.whatwg.org/specs/web-apps/current-work/#exposed
  for (HTMLObjectElement* object =
           Traversal<HTMLObjectElement>::FirstAncestor(*this);
       object; object = Traversal<HTMLObjectElement>::FirstAncestor(*object)) {
    if (object->IsExposed())
      return false;
  }
  return true;
}

}  // namespace blink
