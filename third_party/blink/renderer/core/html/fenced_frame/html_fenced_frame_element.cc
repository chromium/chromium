// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_mparch_delegate.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_shadow_dom_delegate.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kFencedframeTag, document),
      frame_delegate_(FencedFrameDelegate::Create(this)) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kHTMLFencedFrameElement);
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
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

  return MakeGarbageCollected<FencedFrameMPArchDelegate>(outer_element);
}

HTMLFencedFrameElement::FencedFrameDelegate::~FencedFrameDelegate() = default;

void HTMLFencedFrameElement::FencedFrameDelegate::Trace(
    Visitor* visitor) const {
  visitor->Trace(outer_element_);
}

// END HTMLFencedFrameElement::FencedFrameDelegate.

Node::InsertionNotificationRequest HTMLFencedFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLFrameOwnerElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFencedFrameElement::DidNotifySubtreeInsertionsToDocument() {
  frame_delegate_->DidGetInserted();
  Navigate();
}

void HTMLFencedFrameElement::RemovedFrom(ContainerNode& node) {
  // We should verify that the underlying frame has already been disconnected.
  DCHECK_EQ(ContentFrame(), nullptr);
  HTMLFrameOwnerElement::RemovedFrom(node);
}

void HTMLFencedFrameElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == html_names::kSrcAttr) {
    Navigate();
  } else {
    HTMLFrameOwnerElement::ParseAttribute(params);
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

void HTMLFencedFrameElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);

  if (features::kFencedFramesImplementationTypeParam.Get() ==
      features::FencedFramesImplementationType::kMPArch) {
    if (GetLayoutEmbeddedContent() && ContentFrame()) {
      SetEmbeddedContentView(ContentFrame()->View());
    }
  }
}

LayoutObject* HTMLFencedFrameElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy_layout) {
  if (features::kFencedFramesImplementationTypeParam.Get() ==
      features::FencedFramesImplementationType::kMPArch) {
    return new LayoutIFrame(this);
  }

  return HTMLFrameOwnerElement::CreateLayoutObject(style, legacy_layout);
}

}  // namespace blink
