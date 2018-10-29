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

#include "third_party/blink/renderer/core/css_property_names.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_image_loader.h"
#include "third_party/blink/renderer/core/html/html_object_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/plugin_document.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_content.h"
#include "third_party/blink/renderer/core/layout/layout_embedded_object.h"

namespace blink {

using namespace HTMLNames;

inline HTMLEmbedElement::HTMLEmbedElement(Document& document,
                                          const CreateElementFlags flags)
    : HTMLPlugInElement(embedTag,
                        document,
                        flags,
                        kShouldPreferPlugInsForImages) {}

HTMLEmbedElement* HTMLEmbedElement::Create(Document& document,
                                           const CreateElementFlags flags) {
  auto* element = new HTMLEmbedElement(document, flags);
  element->EnsureUserAgentShadowRoot();
  return element;
}

const HashSet<AtomicString>& HTMLEmbedElement::GetCheckedAttributeNames()
    const {
  DEFINE_STATIC_LOCAL(HashSet<AtomicString>, attribute_set, ({"src"}));
  return attribute_set;
}

static inline LayoutEmbeddedContent* FindPartLayoutObject(const Node* n) {
  if (!n->GetLayoutObject())
    n = Traversal<HTMLObjectElement>::FirstAncestor(*n);

  if (n && n->GetLayoutObject() &&
      n->GetLayoutObject()->IsLayoutEmbeddedContent())
    return ToLayoutEmbeddedContent(n->GetLayoutObject());

  return nullptr;
}

LayoutEmbeddedContent* HTMLEmbedElement::ExistingLayoutEmbeddedContent() const {
  return FindPartLayoutObject(this);
}

bool HTMLEmbedElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == hiddenAttr)
    return true;
  return HTMLPlugInElement::IsPresentationAttribute(name);
}

void HTMLEmbedElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == hiddenAttr) {
    if (DeprecatedEqualIgnoringCase(value, "yes") ||
        DeprecatedEqualIgnoringCase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyWidth, 0, CSSPrimitiveValue::UnitType::kPixels);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyHeight, 0, CSSPrimitiveValue::UnitType::kPixels);
    }
  } else {
    HTMLPlugInElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLEmbedElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == typeAttr) {
    SetServiceType(params.new_value.LowerASCII());
    wtf_size_t pos = service_type_.Find(";");
    if (pos != kNotFound)
      SetServiceType(service_type_.Left(pos));
    if (GetLayoutObject()) {
      SetNeedsPluginUpdate(true);
      GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
          "Embed type changed");
    }
  } else if (params.name == codeAttr) {
    // TODO(schenney): Remove this branch? It's not in the spec and we're not in
    // the HTMLAppletElement hierarchy.
    SetUrl(StripLeadingAndTrailingHTMLSpaces(params.new_value));
  } else if (params.name == srcAttr) {
    SetUrl(StripLeadingAndTrailingHTMLSpaces(params.new_value));
    if (GetLayoutObject() && IsImageType()) {
      if (!image_loader_)
        image_loader_ = HTMLImageLoader::Create(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else if (GetLayoutObject()) {
      // Check if this Embed can transition from potentially-active to active
      if (FastHasAttribute(typeAttr)) {
        SetNeedsPluginUpdate(true);
        LazyReattachIfNeeded();
      }
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

  if (url_.IsEmpty() && service_type_.IsEmpty())
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
    url_ = overriden_url.GetString();
    SetServiceType("text/html");
  }

  RequestObject(plugin_params);
}

bool HTMLEmbedElement::LayoutObjectIsNeeded(const ComputedStyle& style) const {
  if (IsImageType())
    return HTMLPlugInElement::LayoutObjectIsNeeded(style);

  // https://html.spec.whatwg.org/multipage/embedded-content.html#the-embed-element
  // While any of the following conditions are occurring, any plugin
  // instantiated for the element must be removed, and the embed element
  // represents nothing:

  // * The element has neither a src attribute nor a type attribute.
  if (!FastHasAttribute(srcAttr) && !FastHasAttribute(typeAttr))
    return false;

  // * The element has a media element ancestor.
  // -> It's realized by LayoutMedia::isChildAllowed.

  // * The element has an ancestor object element that is not showing its
  //   fallback content.
  ContainerNode* p = parentNode();
  if (auto* object = ToHTMLObjectElementOrNull(p)) {
    if (!object->WillUseFallbackContentAtLayout() &&
        !object->UseFallbackContent()) {
      return false;
    }
  }
  return HTMLPlugInElement::LayoutObjectIsNeeded(style);
}

bool HTMLEmbedElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == srcAttr ||
         HTMLPlugInElement::IsURLAttribute(attribute);
}

const QualifiedName& HTMLEmbedElement::SubResourceAttributeName() const {
  return srcAttr;
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
