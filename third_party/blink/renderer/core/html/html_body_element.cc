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
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLBodyElement::HTMLBodyElement(Document& document)
    : HTMLElement(html_names::kBodyTag, document) {}

HTMLBodyElement::~HTMLBodyElement() = default;

bool HTMLBodyElement::IsPresentationAttribute(const QualifiedName& name) const {
  if (name == html_names::kBackgroundAttr ||
      name == html_names::kMarginwidthAttr ||
      name == html_names::kLeftmarginAttr ||
      name == html_names::kMarginheightAttr ||
      name == html_names::kTopmarginAttr || name == html_names::kBgcolorAttr ||
      name == html_names::kTextAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLBodyElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kBackgroundAttr) {
    String url = StripLeadingAndTrailingHTMLSpaces(value);
    if (!url.IsEmpty()) {
      CSSImageValue* image_value =
          CSSImageValue::Create(url, GetDocument().CompleteURL(url),
                                Referrer(GetDocument().OutgoingReferrer(),
                                         GetDocument().GetReferrerPolicy()),
                                OriginClean::kTrue);
      image_value->SetInitiator(localName());
      style->SetProperty(
          CSSPropertyValue(GetCSSPropertyBackgroundImage(), *image_value));
    }
  } else if (name == html_names::kMarginwidthAttr ||
             name == html_names::kLeftmarginAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginRight, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginLeft, value);
  } else if (name == html_names::kMarginheightAttr ||
             name == html_names::kTopmarginAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginBottom, value);
    AddHTMLLengthToStyle(style, CSSPropertyID::kMarginTop, value);
  } else if (name == html_names::kBgcolorAttr) {
    AddHTMLColorToStyle(style, CSSPropertyID::kBackgroundColor, value);
  } else if (name == html_names::kTextAttr) {
    AddHTMLColorToStyle(style, CSSPropertyID::kColor, value);
  } else {
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLBodyElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kVlinkAttr || name == html_names::kAlinkAttr ||
      name == html_names::kLinkAttr) {
    if (value.IsNull()) {
      if (name == html_names::kLinkAttr)
        GetDocument().GetTextLinkColors().ResetLinkColor();
      else if (name == html_names::kVlinkAttr)
        GetDocument().GetTextLinkColors().ResetVisitedLinkColor();
      else
        GetDocument().GetTextLinkColors().ResetActiveLinkColor();
    } else {
      Color color;
      String string_value = value;
      if (!HTMLElement::ParseColorWithLegacyRules(string_value, color))
        return;

      if (name == html_names::kLinkAttr)
        GetDocument().GetTextLinkColors().SetLinkColor(color);
      else if (name == html_names::kVlinkAttr)
        GetDocument().GetTextLinkColors().SetVisitedLinkColor(color);
      else
        GetDocument().GetTextLinkColors().SetActiveLinkColor(color);
    }

    SetNeedsStyleRecalc(kSubtreeStyleChange,
                        StyleChangeReasonForTracing::Create(
                            style_change_reason::kLinkColorChange));
  } else if (name == html_names::kOnafterprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kAfterprint,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnbeforeprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBeforeprint,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kLoad,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnbeforeunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBeforeunload,
        CreateAttributeEventListener(
            GetDocument().GetFrame(), name, value,
            JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler));
  } else if (name == html_names::kOnunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kUnload,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnpagehideAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPagehide,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnpageshowAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPageshow,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnpopstateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPopstate,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnblurAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBlur,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kError,
        CreateAttributeEventListener(
            GetDocument().GetFrame(), name, value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else if (name == html_names::kOnfocusAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocus,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (RuntimeEnabledFeatures::OrientationEventEnabled() &&
             name == html_names::kOnorientationchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOrientationchange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnhashchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kHashchange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnmessageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kMessage,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnmessageerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kMessageerror,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnresizeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kResize,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnscrollAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kScroll,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnselectionchangeAttr) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLBodyElementOnSelectionChangeAttribute);
    GetDocument().SetAttributeEventListener(
        event_type_names::kSelectionchange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnstorageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kStorage,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnonlineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOnline,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnofflineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOffline,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnlanguagechangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kLanguagechange,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (RuntimeEnabledFeatures::PortalsEnabled() &&
             name == html_names::kOnportalactivateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPortalactivate,
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
      SetIntegralAttribute(html_names::kMarginwidthAttr, margin_width);
    if (margin_height != -1)
      SetIntegralAttribute(html_names::kMarginheightAttr, margin_height);
  }
}

bool HTMLBodyElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kBackgroundAttr ||
         HTMLElement::IsURLAttribute(attribute);
}

bool HTMLBodyElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == html_names::kBackgroundAttr ||
         HTMLElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLBodyElement::SubResourceAttributeName() const {
  return html_names::kBackgroundAttr;
}

bool HTMLBodyElement::SupportsFocus() const {
  // This override is needed because the inherited method bails if the parent is
  // editable.  The <body> should be focusable even if <html> is editable.
  return HasEditableStyle(*this) || HTMLElement::SupportsFocus();
}

}  // namespace blink
