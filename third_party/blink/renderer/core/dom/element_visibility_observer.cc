// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/dom/element_visibility_observer.h"

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/intersection_observer/intersection_observer_entry.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

ElementVisibilityObserver::ElementVisibilityObserver(
    Element* element,
    VisibilityCallback callback)
    : element_(element), callback_(std::move(callback)) {}

ElementVisibilityObserver::~ElementVisibilityObserver() = default;

void ElementVisibilityObserver::Start(float threshold) {
  DCHECK(!intersection_observer_);

  ExecutionContext* context = element_->GetExecutionContext();
  Document& document = To<Document>(*context);

  intersection_observer_ = IntersectionObserver::Create(
      {} /* root_margin */, {threshold}, &document,
      WTF::BindRepeating(&ElementVisibilityObserver::OnVisibilityChanged,
                         WrapWeakPersistent(this)));
  DCHECK(intersection_observer_);

  intersection_observer_->observe(element_.Release());
}

void ElementVisibilityObserver::Stop() {
  DCHECK(intersection_observer_);

  intersection_observer_->disconnect();
  intersection_observer_ = nullptr;
}

void ElementVisibilityObserver::DeliverObservationsForTesting() {
  intersection_observer_->Deliver();
}

void ElementVisibilityObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  visitor->Trace(intersection_observer_);
}

void ElementVisibilityObserver::OnVisibilityChanged(
    const HeapVector<Member<IntersectionObserverEntry>>& entries) {
  bool is_visible = entries.back()->intersectionRatio() >=
                    intersection_observer_->thresholds()[0];
  callback_.Run(is_visible);
}

}  // namespace blink
