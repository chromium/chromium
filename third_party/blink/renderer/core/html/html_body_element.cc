/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
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

#include "third_party/blink/renderer/core/html/html_body_element.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/css/css_image_value.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/use_counter.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"

namespace blink {

using namespace HTMLNames;

inline HTMLBodyElement::HTMLBodyElement(Document& document)
    : HTMLElement(bodyTag, document) {}

DEFINE_NODE_FACTORY(HTMLBodyElement)

HTMLBodyElement::~HTMLBodyElement() = default;

bool HTMLBodyElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == backgroundAttr || name == marginwidthAttr ||
      name == leftmarginAttr || name == marginheightAttr ||
      name == topmarginAttr || name == bgcolorAttr || name == textAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLBodyElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == backgroundAttr) {
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (!url.IsEmpty()) {
      CSSImageValue* image_value =
          CSSImageValue::Create(url, GetDocument().CompleteURL(url),
                                Referrer(GetDocument().OutgoingReferrer(),
                                         GetDocument().GetReferrerPolicy()));
      image_value->SetInitiator(localName());
      style->SetProperty(
          CSSPropertyValue(GetCSSPropertyBackgroundImage(), *image_value));
    }
  } else if (name == marginwidthAttr || name == leftmarginAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
  } else if (name == marginheightAttr || name == topmarginAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
  } else if (name == bgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyBackgroundColor, value);
  } else if (name == textAttr) {
    AddHTMLColorToStyle(style, CSSPropertyColor, value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLBodyElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == vlinkAttr || name == alinkAttr || name == linkAttr) {
    if (value.IsNull()) {
      if (name == linkAttr)
        GetDocument().GetTextLinkColors().ResetLinkColor();
      else if (name == vlinkAttr)
        GetDocument().GetTextLinkColors().ResetVisitedLinkColor();
      else
        GetDocument().GetTextLinkColors().ResetActiveLinkColor();
    } else {
      Color color;
      String string_value = value;
      if (!HTMLElement::ParseColorWithLegacyRules(string_value, color))
        return;

      if (name == linkAttr)
        GetDocument().GetTextLinkColors().SetLinkColor(color);
      else if (name == vlinkAttr)
        GetDocument().GetTextLinkColors().SetVisitedLinkColor(color);
      else
        GetDocument().GetTextLinkColors().SetActiveLinkColor(color);
    }

    SetNeedsStyleRecalc(kSubtreeStyleChange,
                        StyleChangeReasonForTracing::Create(
                            style_change_reason::kLinkColorChange));
  } else if (name == onafterprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::afterprint,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onbeforeprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::beforeprint,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::load,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onbeforeunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::beforeunload,
        CreateAttributeEventListener(
            GetDocument().GetFrame(), name, value,
            JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler));
  } else if (name == onunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::unload,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onpagehideAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::pagehide,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onpageshowAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::pageshow,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onpopstateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::popstate,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onblurAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::blur,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::error,
        CreateAttributeEventListener(
            GetDocument().GetFrame(), name, value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else if (name == onfocusAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::focus,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (RuntimeEnabledFeatures::OrientationEventEnabled() &&
             name == onorientationchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::orientationchange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onhashchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::hashchange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onmessageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::message,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onmessageerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::messageerror,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onresizeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::resize,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onscrollAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::scroll,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onselectionchangeAttr) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLBodyElementOnSelectionChangeAttribute);
    GetDocument().SetAttributeEventListener(
        EventTypeNames::selectionchange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onstorageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::storage,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == ononlineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::online,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onofflineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::offline,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == onlanguagechangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        EventTypeNames::languagechange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

Node::InsertionNotificationRequest HTMLBodyElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLBodyElement::DidNotifySubtreeInsertionsToDocument() {
  // FIXME: It's surprising this is web compatible since it means a
  // marginwidth and marginheight attribute can magically appear on the <body>
  // of all documents embedded through <iframe> or <frame>.
  if (GetDocument().GetFrame() && GetDocument().GetFrame()->Owner()) {
    int margin_width = GetDocument().GetFrame()->Owner()->MarginWidth();
    int margin_height = GetDocument().GetFrame()->Owner()->MarginHeight();
    if (margin_width != -1)
      SetIntegralAttribute(marginwidthAttr, margin_width);
    if (margin_height != -1)
      SetIntegralAttribute(marginheightAttr, margin_height);
  }
}

bool HTMLBodyElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == backgroundAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLBodyElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == backgroundAttr || HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLBodyElement::SubResourceAttributeName() const {
  return backgroundAttr;
}

bool HTMLBodyElement::SupportsFocus() const {
  // This override is needed because the inherited method bails if the parent is
  // editable.  The <body> should be focusable even if <html> is editable.
  return HasEditableStyle(*this) || HTMLElement::SupportsFocus();
}

}  // namespace blink
