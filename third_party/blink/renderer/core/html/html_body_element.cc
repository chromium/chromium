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

#include "third_party/blink/renderer/bindings/core/v8/js_event_handler_for_content_attribute.h"
#include "third_party/blink/renderer/core/css/css_property_value_set.h"
#include "third_party/blink/renderer/core/css/parser/css_parser.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/attribute.h"
#include "third_party/blink/renderer/core/editing/editing_utilities.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/html/html_frame_element_base.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
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
    AddHTMLBackgroundImageToStyle(style, value, localName());
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
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnbeforeprintAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBeforeprint,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kLoad, JSEventHandlerForContentAttribute::Create(
                                     GetExecutionContext(), name, value));
  } else if (name == html_names::kOnbeforeunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBeforeunload,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), name, value,
            JSEventHandler::HandlerType::kOnBeforeUnloadEventHandler));
  } else if (name == html_names::kOnunloadAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kUnload, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnpagehideAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPagehide, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (name == html_names::kOnpageshowAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPageshow, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (name == html_names::kOnpopstateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPopstate, JSEventHandlerForContentAttribute::Create(
                                         GetExecutionContext(), name, value));
  } else if (name == html_names::kOnblurAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kBlur, JSEventHandlerForContentAttribute::Create(
                                     GetExecutionContext(), name, value));
  } else if (name == html_names::kOnerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kError,
        JSEventHandlerForContentAttribute::Create(
            GetExecutionContext(), name, value,
            JSEventHandler::HandlerType::kOnErrorEventHandler));
  } else if (name == html_names::kOnfocusAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocus, JSEventHandlerForContentAttribute::Create(
                                      GetExecutionContext(), name, value));
  } else if (RuntimeEnabledFeatures::OrientationEventEnabled() &&
             name == html_names::kOnorientationchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOrientationchange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnhashchangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kHashchange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnmessageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kMessage, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnmessageerrorAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kMessageerror,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnresizeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kResize, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnscrollAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kScroll, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnselectionchangeAttr) {
    UseCounter::Count(GetDocument(),
                      WebFeature::kHTMLBodyElementOnSelectionChangeAttribute);
    GetDocument().SetAttributeEventListener(
        event_type_names::kSelectionchange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (name == html_names::kOnstorageAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kStorage, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnonlineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOnline, JSEventHandlerForContentAttribute::Create(
                                       GetExecutionContext(), name, value));
  } else if (name == html_names::kOnofflineAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kOffline, JSEventHandlerForContentAttribute::Create(
                                        GetExecutionContext(), name, value));
  } else if (name == html_names::kOnlanguagechangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kLanguagechange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else if (RuntimeEnabledFeatures::TimeZoneChangeEventEnabled() &&
             name == html_names::kOntimezonechangeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kTimezonechange,
        JSEventHandlerForContentAttribute::Create(GetExecutionContext(), name,
                                                  value));
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

Node::InsertionNotificationRequest HTMLBodyElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  HTMLBodyElement* body = GetDocument().FirstBodyElement();
  // If the inserted body becomes the first body which may be the viewport
  // defining element, an existing body may no longer propagate overflow to the
  // viewport and establish its own scroll container. Mark that body for style
  // update in case it stops being a viewport defining element.
  if (body == this) {
    if ((body = Traversal<HTMLBodyElement>::NextSibling(*body)))
      GetDocument().GetStyleEngine().FirstBodyElementChanged(body);
  }
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLBodyElement::RemovedFrom(ContainerNode& insertion_point) {
  HTMLElement::RemovedFrom(insertion_point);

  if (insertion_point != GetDocument().documentElement())
    return;

  // Mark remaining body for overflow update since it may change its used values
  // for scrolling due to viewport propagation if the removed body used to be
  // the viewport defining element.
  GetDocument().GetStyleEngine().FirstBodyElementChanged(
      GetDocument().FirstBodyElement());
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

}  // namespace blink
