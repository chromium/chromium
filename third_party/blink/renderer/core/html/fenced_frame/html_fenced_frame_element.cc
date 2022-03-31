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

mojom::blink::FencedFrameMode GetModeAttributeValue(const String& value) {
  // Keep this in sync with the values in the `FencedFrameMode` enum.
  if (EqualIgnoringASCIICase(value, "opaque-ads"))
    return mojom::blink::FencedFrameMode::kOpaqueAds;
  return mojom::blink::FencedFrameMode::kDefault;
}

String FencedFrameModeToString(mojom::blink::FencedFrameMode mode) {
  switch (mode) {
    case mojom::blink::FencedFrameMode::kDefault:
      return "default";
    case mojom::blink::FencedFrameMode::kOpaqueAds:
      return "opaque-ads";
  }

  NOTREACHED();
  return "";
}

}  // namespace

HTMLFencedFrameElement::HTMLFencedFrameElement(Document& document)
    : HTMLFrameOwnerElement(html_names::kFencedframeTag, document) {
  DCHECK(RuntimeEnabledFeatures::FencedFramesEnabled(GetExecutionContext()));
  UseCounter::Count(document, WebFeature::kHTMLFencedFrameElement);
  StartResizeObserver();
}

HTMLFencedFrameElement::~HTMLFencedFrameElement() = default;

void HTMLFencedFrameElement::Trace(Visitor* visitor) const {
  HTMLFrameOwnerElement::Trace(visitor);
  visitor->Trace(frame_delegate_);
  visitor->Trace(resize_observer_);
}

void HTMLFencedFrameElement::DisconnectContentFrame() {
  DCHECK(!GetDocument().IsPrerendering());

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

  // If the element has been disconnected by the time we attempt to create the
  // delegate (eg, due to deferral while prerendering), we should not create the
  // delegate.
  //
  // NB: this check should remain at the beginning of this function so that the
  // remainder of the function can safely assume the frame is connected.
  if (!outer_element->isConnected()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame when disconnected."));
    return nullptr;
  }

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

  if (!SubframeLoadingDisabler::CanLoadFrame(*outer_element)) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. Subframe loading disabled."));
    return nullptr;
  }

  // The frame limit only needs to be checked on initial creation before
  // attempting to insert it into the DOM. This behavior matches how iframes
  // handles frame limits.
  if (!outer_element->IsCurrentlyWithinFrameLimit()) {
    outer_element->GetDocument().AddConsoleMessage(
        MakeGarbageCollected<ConsoleMessage>(
            mojom::blink::ConsoleMessageSource::kJavaScript,
            mojom::blink::ConsoleMessageLevel::kWarning,
            "Can't create a fenced frame. Frame limit exceeded."));
    return nullptr;
  }

  // We must be connected at this point due to the isConnected check at the top
  // of this function.
  DCHECK(outer_element->GetDocument().GetFrame());
  if (Frame* ancestor = outer_element->GetDocument().GetFrame()) {
    mojom::blink::FencedFrameMode current_mode = outer_element->GetMode();
    // This loop is only relevant for fenced frames based on ShadowDOM, since it
    // has to do with the `FramePolicy::is_fenced` bit. We have to keep
    // traversing up the tree to see if we ever come across a fenced frame of
    // another mode. In that case, we stop `this` frame from being fully
    // created, since nested fenced frames of differing modes are not allowed.
    while (ancestor && ancestor->Owner()) {
      bool is_ancestor_fenced = ancestor->Owner()->GetFramePolicy().is_fenced;
      // Note that this variable is only meaningful if `is_ancestor_fenced`
      // above is true.
      mojom::blink::FencedFrameMode ancestor_mode =
          ancestor->Owner()->GetFramePolicy().fenced_frame_mode;

      if (is_ancestor_fenced && ancestor_mode != current_mode) {
        outer_element->GetDocument().AddConsoleMessage(
            MakeGarbageCollected<ConsoleMessage>(
                mojom::blink::ConsoleMessageSource::kJavaScript,
                mojom::blink::ConsoleMessageLevel::kWarning,
                "Cannot create a fenced frame with mode '" +
                    FencedFrameModeToString(current_mode) +
                    "' nested in a fenced frame with mode '" +
                    FencedFrameModeToString(ancestor_mode) + "'."));
        return nullptr;
      }

      // If this loop found a fenced ancestor whose mode is compatible with
      // `current_mode`, it is not necessary to look further up the ancestor
      // chain. This is because this loop already ran during the creation of
      // the compatible fenced ancestor, so it is guaranteed that the rest of
      // the ancestor chain has already been checked and approved for
      // compatibility.
      if (is_ancestor_fenced && ancestor_mode == current_mode) {
        break;
      }

      ancestor = ancestor->Tree().Parent();
    }
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
  CreateDelegateAndNavigate();
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
  if (params.name == html_names::kModeAttr) {
    mojom::blink::FencedFrameMode new_mode =
        GetModeAttributeValue(params.new_value);
    if (new_mode != mode_ && freeze_mode_attribute_) {
      GetDocument().AddConsoleMessage(MakeGarbageCollected<ConsoleMessage>(
          mojom::blink::ConsoleMessageSource::kJavaScript,
          mojom::blink::ConsoleMessageLevel::kWarning,
          "Changing the `mode` attribute on a fenced frame has no effect after "
          "it has already been frozen due to the first navigation."));
      return;
    }

    mode_ = new_mode;
  } else if (params.name == html_names::kSrcAttr) {
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

  // Please see HTMLFencedFrameDelegate::Create for a list of conditions which
  // could result in not having a frame delegate at this point, one of which is
  // prerendering. If this function is called while prerendering we won't have a
  // delegate and will bail early, but this should still be correct since,
  // post-activation, CreateDelegateAndNavigate will be run which will navigate
  // to the most current src.
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

  // Freeze the `mode` attribute to its current value even if it has never been
  // explicitly set before, so that it cannot change after the first navigation.
  freeze_mode_attribute_ = true;

  if (!frozen_frame_size_)
    FreezeFrameSize();
}

void HTMLFencedFrameElement::CreateDelegateAndNavigate() {
  // We may queue up several calls to CreateDelegateAndNavigate while
  // prerendering, but we should only actually create the delegate once. Note,
  // this will also mean that we skip calling Navigate() again, but the result
  // should still be correct since the first Navigate call will use the
  // up-to-date src.
  if (frame_delegate_)
    return;
  if (GetDocument().IsPrerendering()) {
    GetDocument().AddPostPrerenderingActivationStep(
        WTF::Bind(&HTMLFencedFrameElement::CreateDelegateAndNavigate,
                  WrapWeakPersistent(this)));
    return;
  }
  frame_delegate_ = FencedFrameDelegate::Create(this);
  Navigate();
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

  FreezeFrameSize(content_rect_->size);
}

void HTMLFencedFrameElement::FreezeFrameSize(const PhysicalSize& size) {
  DCHECK(!frozen_frame_size_);
  frozen_frame_size_ = content_rect_->size;

  if (!features::IsFencedFramesMPArchBased()) {
    // With Shadow DOM, update the CSS `transform` property whenever
    // |content_rect_| or |frozen_frame_size_| change.
    UpdateInnerStyleOnFrozenInternalFrame();
    return;
  }
  // Stop the `ResizeObserver` when frozen. It is needed only to compute the
  // frozen size.
  StopResizeObserver();
}

void HTMLFencedFrameElement::StartResizeObserver() {
  DCHECK(!resize_observer_);
  resize_observer_ =
      ResizeObserver::Create(GetDocument().domWindow(),
                             MakeGarbageCollected<ResizeObserverDelegate>());
  resize_observer_->observe(this);
}

void HTMLFencedFrameElement::StopResizeObserver() {
  if (!resize_observer_)
    return;
  resize_observer_->disconnect();
  resize_observer_ = nullptr;
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
    FreezeFrameSize(content_rect_->size);
    return;
  }
  if (frozen_frame_size_ && features::IsFencedFramesMPArchBased())
    UpdateInnerStyleOnFrozenInternalFrame();
}

void HTMLFencedFrameElement::UpdateInnerStyleOnFrozenInternalFrame() {
  DCHECK(!features::IsFencedFramesMPArchBased());
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
