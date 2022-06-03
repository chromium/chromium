// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/document_transition/document_transition_container_element.h"

#include "third_party/blink/renderer/core/css/css_property_names.h"
#include "third_party/blink/renderer/core/css/properties/computed_style_utils.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"

namespace blink {

class DocumentTransitionContainerElement::ResizeObserverDelegate final
    : public ResizeObserver::Delegate {
 public:
  explicit ResizeObserverDelegate(
      DocumentTransitionContainerElement* container_element,
      DocumentTransitionContentElement* content_element)
      : container_element_(container_element),
        content_element_(content_element) {
    DCHECK(container_element);
    DCHECK(content_element);
  }
  ~ResizeObserverDelegate() override = default;

  // Invoked when the target shared element is resized.
  void OnResize(
      const HeapVector<Member<ResizeObserverEntry>>& entries) override {
    DCHECK_EQ(1u, entries.size());

    auto border_box_size = entries[0]->borderBoxSize()[0];
    auto layout_size = LayoutSize(DoubleSize(border_box_size->inlineSize(),
                                             border_box_size->blockSize()));

    content_element_->SetIntrinsicSize(layout_size);
    container_element_->WasResized(layout_size);
  }

  void Trace(Visitor* visitor) const override {
    visitor->Trace(container_element_);
    visitor->Trace(content_element_);
    ResizeObserver::Delegate::Trace(visitor);
  }

 private:
  Member<DocumentTransitionContainerElement> container_element_;
  Member<DocumentTransitionContentElement> content_element_;
};

DocumentTransitionContainerElement::DocumentTransitionContainerElement(
    Document& document)
    : HTMLElement(QualifiedName(g_null_atom,
                                "transition_container_element",
                                g_null_atom),
                  document) {
  // TODO(khushalsagar) : Move this to a UA style sheet.
  SetInlineStyleProperty(CSSPropertyID::kPosition, CSSValueID::kFixed);
  SetInlineStyleProperty(CSSPropertyID::kTop, 0,
                         CSSPrimitiveValue::UnitType::kPixels);
  SetInlineStyleProperty(CSSPropertyID::kLeft, 0,
                         CSSPrimitiveValue::UnitType::kPixels);
  SetInlineStyleProperty(CSSPropertyID::kBoxSizing, CSSValueID::kBorderBox);

  // Using CSS transition ensures that the animation is retargeted if the state
  // of the live element changes.
  SetInlineStyleProperty(CSSPropertyID::kTransition,
                         "all 0.25s cubic-bezier(0.4, 0.0, 0.2, 1.0)");

  // TODO(khushalsagar) : These pseudo elements will need to be associated with
  // a persistent DOM element (html or body element). See crbug.com/1265698.
  document.body()->AppendChild(this);
}

DocumentTransitionContainerElement::~DocumentTransitionContainerElement() =
    default;

void DocumentTransitionContainerElement::Prepare(Element* target_element) {
  DCHECK_EQ(state_, State::kIdle);
  state_ = State::kPreparing;

  if (target_element) {
    old_content_ =
        MakeGarbageCollected<DocumentTransitionContentElement>(GetDocument());
    AppendChild(old_content_);

    // TODO(khushalsagar) : We need an observer for the element's transform
    // similar to its size.
    resize_observer_ = ResizeObserver::Create(
        GetDocument().domWindow(),
        MakeGarbageCollected<ResizeObserverDelegate>(this, old_content_));
    resize_observer_->observe(target_element);

    target_element_ = target_element;
    UpdateTransform();
  }
}

void DocumentTransitionContainerElement::PrepareResolved() {
  DCHECK_EQ(state_, State::kPreparing);
  state_ = State::kPrepared;

  if (resize_observer_) {
    resize_observer_->disconnect();
    resize_observer_ = nullptr;
    target_element_ = nullptr;
  }
}

void DocumentTransitionContainerElement::Start(Element* target_element) {
  DCHECK_EQ(state_, State::kPrepared);
  state_ = State::kStarted;

  if (target_element) {
    new_content_ =
        MakeGarbageCollected<DocumentTransitionContentElement>(GetDocument());
    AppendChild(new_content_);

    resize_observer_ = ResizeObserver::Create(
        GetDocument().domWindow(),
        MakeGarbageCollected<ResizeObserverDelegate>(this, new_content_));
    resize_observer_->observe(target_element);

    target_element_ = target_element;
    UpdateTransform();
  }

  DCHECK(old_content_ || new_content_)
      << "One of old or new content must be provided";
}

void DocumentTransitionContainerElement::StartFinished() {
  DCHECK_EQ(state_, State::kStarted);
  state_ = State::kFinished;

  GetDocument().body()->removeChild(this);

  if (resize_observer_) {
    resize_observer_->disconnect();
    resize_observer_ = nullptr;
  }

  target_element_ = nullptr;
}

void DocumentTransitionContainerElement::WasResized(
    const LayoutSize& new_size) {
  SetInlineStyleProperty(CSSPropertyID::kWidth, new_size.Width(),
                         CSSPrimitiveValue::UnitType::kPixels);
  SetInlineStyleProperty(CSSPropertyID::kHeight, new_size.Height(),
                         CSSPrimitiveValue::UnitType::kPixels);
}

void DocumentTransitionContainerElement::UpdateTransform() {
  if (!target_element_)
    return;

  if (auto* layout_object = target_element_->GetLayoutObject()) {
    auto transform = layout_object->LocalToAbsoluteTransform();
    if (transform == container_transform_)
      return;

    container_transform_ = transform;
    SetInlineStyleProperty(
        CSSPropertyID::kTransform,
        ComputedStyleUtils::ValueForTransformationMatrix(transform, 1, false)
            ->CssText());
  }
}

void DocumentTransitionContainerElement::Trace(Visitor* visitor) const {
  visitor->Trace(old_content_);
  visitor->Trace(new_content_);
  visitor->Trace(resize_observer_);
  visitor->Trace(target_element_);
  HTMLElement::Trace(visitor);
}

}  // namespace blink
