/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Simon Hausmann (hausmann@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2006, 2009, 2010 Apple Inc. All rights reserved.
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

#include "third_party/blink/renderer/core/html/html_frame_set_element.h"

#include "third_party/blink/renderer/bindings/core/v8/script_event_listener.h"
#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/events/mouse_event.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/local_frame_client.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/html_frame_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_frame_set.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

namespace blink {

HTMLFrameSetElement::HTMLFrameSetElement(Document& document)
    : HTMLElement(html_names::kFramesetTag, document),
      border_(6),
      border_set_(false),
      border_color_set_(false),
      frameborder_(true),
      frameborder_set_(false),
      noresize_(false) {
  SetHasCustomStyleCallbacks();
  UseCounter::Count(document, WebFeature::kHTMLFrameSetElement);
}

bool HTMLFrameSetElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kBordercolorAttr)
    return true;
  return HTMLElement::IsPresentationAttribute(name);
}

void HTMLFrameSetElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kBordercolorAttr)
    AddHTMLColorToStyle(style, CSSPropertyID::kBorderColor, value);
  else
    HTMLElement::CollectStyleForPresentationAttribute(name, value, style);
}

void HTMLFrameSetElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == html_names::kRowsAttr) {
    if (!value.IsNull()) {
      row_lengths_ = ParseListOfDimensions(value.GetString());
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
    }
  } else if (name == html_names::kColsAttr) {
    if (!value.IsNull()) {
      col_lengths_ = ParseListOfDimensions(value.GetString());
      SetNeedsStyleRecalc(kSubtreeStyleChange,
                          StyleChangeReasonForTracing::FromAttribute(name));
    }
  } else if (name == html_names::kFrameborderAttr) {
    if (!value.IsNull()) {
      if (DeprecatedEqualIgnoringCase(value, "no") ||
          DeprecatedEqualIgnoringCase(value, "0")) {
        frameborder_ = false;
        frameborder_set_ = true;
      } else if (DeprecatedEqualIgnoringCase(value, "yes") ||
                 DeprecatedEqualIgnoringCase(value, "1")) {
        frameborder_set_ = true;
      }
    } else {
      frameborder_ = false;
      frameborder_set_ = false;
    }
  } else if (name == html_names::kNoresizeAttr) {
    noresize_ = true;
  } else if (name == html_names::kBorderAttr) {
    if (!value.IsNull()) {
      border_ = value.ToInt();
      border_set_ = true;
    } else {
      border_set_ = false;
    }
  } else if (name == html_names::kBordercolorAttr) {
    border_color_set_ = !value.IsEmpty();
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
  } else if (name == html_names::kOnfocusinAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocusin,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnfocusoutAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kFocusout,
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
  } else if (name == html_names::kOnresizeAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kResize,
        CreateAttributeEventListener(GetDocument().GetFrame(), name, value));
  } else if (name == html_names::kOnscrollAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kScroll,
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
  } else if (name == html_names::kOnpopstateAttr) {
    GetDocument().SetWindowAttributeEventListener(
        event_type_names::kPopstate,
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

bool HTMLFrameSetElement::LayoutObjectIsNeeded(
    const ComputedStyle& style) const {
  // For compatibility, frames layoutObject even when display: none is set.
  return true;
}

LayoutObject* HTMLFrameSetElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy) {
  if (style.HasContent())
    return LayoutObject::CreateObject(this, style, legacy);
  return new LayoutFrameSet(this);
}

void HTMLFrameSetElement::AttachLayoutTree(AttachContext& context) {
  // Inherit default settings from parent frameset
  // FIXME: This is not dynamic.
  if (HTMLFrameSetElement* frameset =
          Traversal<HTMLFrameSetElement>::FirstAncestor(*this)) {
    if (!frameborder_set_)
      frameborder_ = frameset->HasFrameBorder();
    if (frameborder_) {
      if (!border_set_)
        border_ = frameset->Border();
      if (!border_color_set_)
        border_color_set_ = frameset->HasBorderColor();
    }
    if (!noresize_)
      noresize_ = frameset->NoResize();
  }

  HTMLElement::AttachLayoutTree(context);
}

void HTMLFrameSetElement::DefaultEventHandler(Event& evt) {
  if (evt.IsMouseEvent() && !noresize_ && GetLayoutObject() &&
      GetLayoutObject()->IsFrameSet()) {
    if (ToLayoutFrameSet(GetLayoutObject())->UserResize(ToMouseEvent(evt))) {
      evt.SetDefaultHandled();
      return;
    }
  }
  HTMLElement::DefaultEventHandler(evt);
}

Node::InsertionNotificationRequest HTMLFrameSetElement::InsertedInto(
    ContainerNode& insertion_point) {
  if (insertion_point.isConnected() && GetDocument().GetFrame()) {
    // A document using <frameset> likely won't literally have a body, but as
    // far as the client is concerned, the frameset is effectively the body.
    GetDocument().WillInsertBody();
  }
  return HTMLElement::InsertedInto(insertion_point);
}
void HTMLFrameSetElement::WillRecalcStyle(const StyleRecalcChange) {
  if (NeedsStyleRecalc() && GetLayoutObject()) {
    GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kStyleChange);
    ClearNeedsStyleRecalc();
  }
}

}  // namespace blink
