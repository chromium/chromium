// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/display_cutout_client_impl.h"

#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/core/core_probe_sink.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/element_traversal.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/viewport_data.h"
#include "third_party/blink/renderer/core/html/html_body_element.h"
#include "third_party/blink/renderer/core/inspector/inspector_audits_issue.h"
#include "third_party/blink/renderer/core/layout/layout_object.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/style/computed_style.h"

namespace blink {

namespace {

// Returns true if the document's layout viewport is non-scrollable, i.e. the
// contents fit within the viewport height.
bool IsViewportNonScrollable(const LocalFrame& frame) {
  Document* document = frame.GetDocument();
  if (!document) {
    return true;
  }
  LayoutView* layout_view = document->GetLayoutView();
  if (!layout_view) {
    return true;
  }
  return !layout_view->HasScrollableOverflowY();
}

// Returns true if any paint-visible element intersects the band
// [0, top_inset_px] at the top of the viewport AND is either pinned to the
// top (fixed, or sticky with a top constraint) or the viewport is
// non-scrollable.
bool HasOccludedTopContent(const LocalFrame& frame, int top_inset_px) {
  Document* document = frame.GetDocument();
  if (!document || !document->body()) {
    return false;
  }
  const bool non_scrollable = IsViewportNonScrollable(frame);
  for (Element& element : ElementTraversal::DescendantsOf(*document->body())) {
    const LayoutObject* layout_object = element.GetLayoutObject();
    if (!layout_object) {
      continue;
    }
    const ComputedStyle& style = layout_object->Style();
    if (style.Visibility() != EVisibility::kVisible || style.Opacity() == 0) {
      continue;
    }
    EPosition position = style.GetPosition();
    // Sticky without a top constraint scrolls away with the content.
    const bool is_pinned_to_top =
        position == EPosition::kFixed ||
        (position == EPosition::kSticky && !style.Top().IsAuto());
    if (!is_pinned_to_top && !non_scrollable) {
      continue;
    }
    gfx::Rect rect = layout_object->AbsoluteBoundingBoxRect();
    if (rect.IsEmpty()) {
      continue;
    }
    if (rect.y() < top_inset_px && rect.bottom() > 0) {
      return true;
    }
  }
  return false;
}

bool ShouldReportMissingSafeAreaInsetTopIssue(LocalFrame& frame,
                                              const gfx::Insets& safe_area) {
  if (safe_area.top() <= 0) {
    return false;
  }
  if (!frame.IsOutermostMainFrame()) {
    return false;
  }
  CoreProbeSink* probe_sink = frame.GetProbeSink();
  if (!probe_sink || !probe_sink->HasInspectorAuditsAgents()) {
    return false;
  }
  Document* document = frame.GetDocument();
  if (!document) {
    return false;
  }
  mojom::blink::ViewportFit viewport_fit =
      document->GetViewportData().GetViewportDescription().GetViewportFit();
  if (viewport_fit != mojom::blink::ViewportFit::kCover &&
      viewport_fit != mojom::blink::ViewportFit::kCoverForcedByUserAgent) {
    return false;
  }
  // The use counter excludes env() usage inside UA shadow trees (e.g. media
  // controls) and covers shadow DOM and pseudo-elements.
  DocumentLoader* loader = document->Loader();
  if (loader && loader->GetUseCounter().IsCounted(
                    WebFeature::kCSSEnvironmentVariable_SafeAreaInsetTop)) {
    return false;
  }
  return HasOccludedTopContent(frame, safe_area.top());
}

}  // namespace

DisplayCutoutClientImpl::DisplayCutoutClientImpl(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient> receiver)
    : frame_(frame),
      receiver_(this, frame->DomWindow()->GetExecutionContext()) {
  receiver_.Bind(std::move(receiver), frame->GetFrameScheduler()->GetTaskRunner(
                                          TaskType::kInternalDefault));
}

void DisplayCutoutClientImpl::BindMojoReceiver(
    LocalFrame* frame,
    mojo::PendingAssociatedReceiver<mojom::blink::DisplayCutoutClient>
        receiver) {
  if (!frame) {
    return;
  }
  MakeGarbageCollected<DisplayCutoutClientImpl>(frame, std::move(receiver));
}

void DisplayCutoutClientImpl::SetSafeArea(const gfx::Insets& safe_area) {
  frame_->GetDocument()->GetPage()->SetMaxSafeAreaInsets(frame_, safe_area);

  // Fire at most one DevTools issue per frame lifetime.
  if (!has_reported_missing_safe_area_inset_top_issue_ &&
      ShouldReportMissingSafeAreaInsetTopIssue(*frame_, safe_area)) {
    AuditsIssue::ReportViewportFitCoverMissingSafeAreaInsetTopIssue(frame_);
    has_reported_missing_safe_area_inset_top_issue_ = true;
  }
}

// static
void DisplayCutoutClientImpl::MaybeReportMissingSafeAreaInsetTopIssue(
    LocalFrame* frame,
    const gfx::Insets& safe_area) {
  if (!frame) {
    return;
  }
  if (ShouldReportMissingSafeAreaInsetTopIssue(*frame, safe_area)) {
    AuditsIssue::ReportViewportFitCoverMissingSafeAreaInsetTopIssue(frame);
  }
}

void DisplayCutoutClientImpl::Trace(Visitor* visitor) const {
  visitor->Trace(frame_);
  visitor->Trace(receiver_);
}

}  // namespace blink
