// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_shadow_dom_delegate.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLElement(html_names::kFencedframeTag, document),
      frame_delegate_(FencedFrameDelegate::Create(this)) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kHTMLFencedFrameElement);
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLElement::Trace(visitor);
  visitor->Trace(frame_delegate_);
}

// START HTMLFencedFrameElement::FencedFrameDelegate.

HTMLFencedFrameElement::FencedFrameDelegate*
HTMLFencedFrameElement::FencedFrameDelegate::Create(
    HTMLFencedFrameElement* outer_element) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(
      outer_element->GetExecutionContext()));
  if (features::kFencedFramesImplementationTypeParam.Get() ==
      features::FencedFramesImplementationType::kShadowDOM) {
    return MakeGarbageCollected<FencedFrameShadowDOMDelegate>(outer_element);
  }

  // TODO(domfarolino): Once we land testable parts of the MPArch
  // implementation, remove this and return the correct delegate. Note that this
  // should be a NOTREACHED(), but for extra consistency we need to trip this on
  // non-debug builds as well, since it is technically possible to hit this code
  // path in a production build with the kFencedFrames feature enabled, and the
  // param set to kMPArch.
  CHECK(false) << "The MPArch path for fenced frames is currently not "
                  "implemented, please do not use the kMPArch feature "
                  "parameter as it is not supported at this time.";
  return nullptr;
}

HTMLFencedFrameElement::FencedFrameDelegate::~FencedFrameDelegate() = default;

void HTMLFencedFrameElement::FencedFrameDelegate::Trace(
    Visitor* visitor) const {
  visitor->Trace(outer_element_);
}

// END HTMLFencedFrameElement::FencedFrameDelegate.

Node::InsertionNotificationRequest HTMLFencedFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFencedFrameElement::DidNotifySubtreeInsertionsToDocument() {
  frame_delegate_->DidGetInserted();
  Navigate();
}

void HTMLFencedFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSrcAttr) {
    Navigate();
  } else {
    HTMLElement::ParseAttribute(params);
  }
}

bool HTMLFencedFrameElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == html_names::kSrcAttr;
}

void HTMLFencedFrameElement::Navigate() {
  if (!isConnected())
    return;

  KURL url = KURL(GetNonEmptyURLAttribute(html_names::kSrcAttr));

  DCHECK(frame_delegate_);
  frame_delegate_->Navigate(url);
}

}  // namespace blink
