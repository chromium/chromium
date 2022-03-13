// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/fenced_frame/html_fenced_frame_element.h"

#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/frame/fenced_frame_sandbox_flags.h"
#include "third_party/blink/public/mojom/devtools/console_message.mojom-blink.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/css/style_change_reason.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/shadow_root.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/geometry/dom_rect_read_only.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_mparch_delegate.h"
#include "third_party/blink/renderer/core/html/fenced_frame/fenced_frame_shadow_dom_delegate.h"
#include "third_party/blink/renderer/core/html/html_iframe_element.h"
#include "third_party/blink/renderer/core/html/html_style_element.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/layout/layout_iframe.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/resize_observer/resize_observer_entry.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"

namespace blink {

namespace {

PhysicalRect ToPhysicalRect(const DOMRectReadOnly& rect) {
  return PhysicalRect(LayoutUnit::FromDoubleRound(rect.x()),
                      LayoutUnit::FromDoubleRound(rect.y()),
                      LayoutUnit::FromDoubleRound(rect.width()),
                      LayoutUnit::FromDoubleRound(rect.height()));
}

}  // namespace

HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kFencedframeTag, document) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kHTMLFencedFrameElement);
  if (!features::IsFencedFramesMPArchBased())
    StartResizeObserver();
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(frame_delegate_);
  visitor->Trace(resize_observer_);
}

void HTMLFencedFrameElement::DisconnectContentFrame() {
  // The `frame_delegate_` will not exist if the element was not allowed to
  // create its underlying frame at insertion-time.
  if (frame_delegate_)
    frame_delegate_->Dispose();
  frame_delegate_ = nullptr;

  HTMLFrameOwnerElement::DisconnectContentFrame();
}

void HTMLFencedFrameElement::SetCollapsed(bool collapse) {
  if (collapsed_by_client_ == collapse)
    return;

  collapsed_by_client_ = collapse;

  // This is always called in response to an IPC, so should not happen in the
  // middle of a style recalc.
  DCHECK(!GetDocument().InStyleRecalc());

  // Trigger style recalc to trigger layout tree re-attachment.
  SetNeedsStyleRecalc(kLocalStyleChange, StyleChangeReasonForTracing::Create(
                                             style_change_reason::kFrame));
}

// START HTMLFencedFrameElement::FencedFrameDelegate.

HTMLFencedFrameElement::FencedFrameDelegate*
HTMLFencedFrameElement::FencedFrameDelegate::Create(
    HTMLFencedFrameElement* outer_element) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(
      outer_element->GetExecutionContext()));

  if (outer_element->GetExecutionContext()->IsSandboxed(
          kFencedFrameMandatoryUnsandboxedFlags)) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. A sandboxed document can load fenced "
            "frames only when all of the following permissions are set: "
            "allow-same-origin, allow-forms, allow-scripts, allow-popups, "
            "allow-popups-to-escape-sandbox and "
            "allow-top-navigation-by-user-activation."));
    return nullptr;
  }

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

HTMLIFrameElement* HTMLFencedFrameElement::InnerIFrameElement() const {
  if (const ShadowRoot* root = UserAgentShadowRoot())
    return To<HTMLIFrameElement>(root->lastChild());
  return nullptr;
}

Node::InsertionNotificationRequest HTMLFencedFrameElement::InsertedInto(
    ContainerNode& insertion_point) {
  HTMLFrameOwnerElement::InsertedInto(insertion_point);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLFencedFrameElement::DidNotifySubtreeInsertionsToDocument() {
  // This method is the only place that sets `frame_delegate_`, and it cannot be
  // called twice before removal.
  DCHECK(!frame_delegate_);

  if (!SubframeLoadingDisabler::CanLoadFrame(*this))
    return;

  // The frame limit only needs to be checked on initial creation before
  // attempting to insert it into the DOM. This behavior matches how iframes
  // handles frame limits.
  if (!IsCurrentlyWithinFrameLimit())
    return;

  frame_delegate_ = FencedFrameDelegate::Create(this);
  Navigate();
}

void HTMLFencedFrameElement::RemovedFrom(ContainerNode& node) {
  // Verify that the underlying frame has already been disconnected via
  // `DisconnectContentFrame()`. This is only relevant for the MPArch
  // implementation.
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

bool HTMLFencedFrameElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == html_names::kWidthAttr || name == html_names::kHeightAttr)
    return true;
  return HTMLFrameOwnerElement::IsPresentationAttribute(name);
}

void HTMLFencedFrameElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == html_names::kWidthAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kWidth, value);
  } else if (name == html_names::kHeightAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyID::kHeight, value);
  } else {
    HTMLFrameOwnerElement::CollectStyleForPresentationAttribute(name, value,
                                                                style);
  }
}

void HTMLFencedFrameElement::Navigate() {
  if (!isConnected())
    return;
  if (!frame_delegate_)
    return;

  KURL url = GetNonEmptyURLAttribute(html_names::kSrcAttr);

  // TODO(crbug.com/1243568): Convert empty URLs to about:blank, and more
  // generally implement the navigation restrictions to potentially-trustworthy
  // URLs + urn:uuids.
  if (url.IsEmpty())
    return;

  if (!GetExecutionContext()->IsSecureContext()) {
    GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
        mojom::blink::ConsoleMessageSource::kRendering,
        mojom::blink::ConsoleMessageLevel::kWarning,
        "A fenced frame was not loaded because the page is not in a secure "
        "context."));
    return;
  }

  frame_delegate_->Navigate(url);

  if (!frozen_frame_size_)
    FreezeFrameSize();
}

void HTMLFencedFrameElement::AttachLayoutTree(AttachContext& context) {
  HTMLFrameOwnerElement::AttachLayoutTree(context);
  if (features::IsFencedFramesMPArchBased()) {
    if (GetLayoutEmbeddedContent() && ContentFrame()) {
      SetEmbeddedContentView(ContentFrame()->View());
    }
  }
}

bool HTMLFencedFrameElement::LayoutObjectIsNeeded(
    const ComputedStyle& style) const {
  return !collapsed_by_client_ &&
         HTMLFrameOwnerElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLFencedFrameElement::CreateLayoutObject(
    const ComputedStyle& style,
    LegacyLayout legacy_layout) {
  if (features::IsFencedFramesMPArchBased()) {
    return MakeGarbageCollected<LayoutIFrame>(this);
  }

  return HTMLFrameOwnerElement::CreateLayoutObject(style, legacy_layout);
}

bool HTMLFencedFrameElement::SupportsFocus() const {
  return features::IsFencedFramesMPArchBased();
}

void HTMLFencedFrameElement::FreezeFrameSize() {
  if (features::IsFencedFramesMPArchBased())
    return;
  DCHECK(!frozen_frame_size_);

  // When the parser finds `<fencedframe>` with the `src` attribute, the
  // |Navigate| occurs after |LayoutObject| tree is created and its initial
  // layout was done (|NeedsLayout| is cleared,) but the size of the `<iframe>`
  // is still (0, 0). Wait until a lifecycle completes and the resize observer
  // runs.
  if (!content_rect_) {
    should_freeze_frame_size_on_next_layout_ = true;
    return;
  }

  frozen_frame_size_ = content_rect_->size;
  UpdateInnerStyleOnFrozenInternalFrame();
}

void HTMLFencedFrameElement::StartResizeObserver() {
  DCHECK(!features::IsFencedFramesMPArchBased());
  DCHECK(!resize_observer_);
  resize_observer_ =
      ResizeObserver::Create(GetDocument().domWindow(),
                             MakeGarbageCollected<ResizeObserverDelegate>());
  resize_observer_->observe(this);
}

void HTMLFencedFrameElement::ResizeObserverDelegate::OnResize(
    const HeapVector<Member<ResizeObserverEntry>>& entries) {
  if (entries.IsEmpty())
    return;
  const Member<ResizeObserverEntry>& entry = entries.back();
  auto* element = To<HTMLFencedFrameElement>(entry->target());
  const DOMRectReadOnly* content_rect = entry->contentRect();
  element->OnResize(ToPhysicalRect(*content_rect));
}

void HTMLFencedFrameElement::OnResize(const PhysicalRect& content_rect) {
  content_rect_ = content_rect;
  // If the size information at |FreezeFrameSize| is not complete and we
  // needed to postpone freezing until the next resize, do it now. See
  // |FreezeFrameSize| for more.
  if (should_freeze_frame_size_on_next_layout_) {
    should_freeze_frame_size_on_next_layout_ = false;
    DCHECK(!frozen_frame_size_);
    frozen_frame_size_ = content_rect_->size;
  }
  if (frozen_frame_size_)
    UpdateInnerStyleOnFrozenInternalFrame();
}

void HTMLFencedFrameElement::UpdateInnerStyleOnFrozenInternalFrame() {
  DCHECK(content_rect_);
  DCHECK(frozen_frame_size_);
  const double child_width = frozen_frame_size_->width.ToDouble();
  const double child_height = frozen_frame_size_->height.ToDouble();
  // TODO(kojii): Theoritically this `transform` is the same as `object-fit:
  // contain`, but `<iframe>` does not support the `object-fit` property today.
  // We can change to use the `object-fit` property and stop the resize-observer
  // once it is supported.
  String css;
  if (child_width <= std::numeric_limits<double>::epsilon() ||
      child_height <= std::numeric_limits<double>::epsilon()) {
    // If the child's width or height is zero, the scale will be infinite. Do
    // not scale in such cases.
    css =
        String::Format("width: %fpx; height: %fpx", child_width, child_height);
  } else {
    const double parent_width = content_rect_->Width().ToDouble();
    const double parent_height = content_rect_->Height().ToDouble();
    const double scale_x = parent_width / child_width;
    const double scale_y = parent_height / child_height;
    const double scale = std::min(scale_x, scale_y);
    const double tx = (parent_width - child_width * scale) / 2;
    const double ty = (parent_height - child_height * scale) / 2;
    css = String::Format(
        "width: %fpx; height: %fpx; transform: translate(%fpx, %fpx) scale(%f)",
        child_width, child_height, tx, ty, scale);
  }
  InnerIFrameElement()->setAttribute(html_names::kStyleAttr, css,
                                     ASSERT_NO_EXCEPTION);
}

}  // namespace blink
