// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_handler.h"

#include "components/shared_highlighting/core/common/shared_highlighting_features.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker.h"
#include "third_party/blink/renderer/core/editing/markers/document_marker_controller.h"
#include "third_party/blink/renderer/core/editing/position_with_affinity.h"
#include "third_party/blink/renderer/core/editing/visible_units.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_anchor.h"

namespace blink {

TextFragmentHandler::TextFragmentHandler(LocalFrame* main_frame)
    : text_fragment_selector_generator_(
          MakeGarbageCollected<TextFragmentSelectorGenerator>(main_frame)) {}

void TextFragmentHandler::BindTextFragmentReceiver(
    mojo::PendingReceiver<mojom::blink::TextFragmentReceiver> producer) {
  selector_producer_.reset();
  selector_producer_.Bind(
      std::move(producer),
      text_fragment_selector_generator_->GetFrame()->GetTaskRunner(
          blink::TaskType::kInternalDefault));
}

TextFragmentSelectorGenerator*
TextFragmentHandler::GetTextFragmentSelectorGenerator() {
  return text_fragment_selector_generator_;
}

void TextFragmentHandler::Cancel() {
  GetTextFragmentSelectorGenerator()->Cancel();
}

void TextFragmentHandler::RequestSelector(RequestSelectorCallback callback) {
  GetTextFragmentSelectorGenerator()->RequestSelector(std::move(callback));
}

void TextFragmentHandler::GetExistingSelectors(
    GetExistingSelectorsCallback callback) {
  Vector<String> text_fragment_selectors;

  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor) {
    std::move(callback).Run(Vector<String>());
    return;
  }

  for (auto& finder : anchor->TextFragmentFinders()) {
    if (finder->FirstMatch()) {
      text_fragment_selectors.push_back(finder->GetSelector().ToString());
    }
  }

  std::move(callback).Run(text_fragment_selectors);
}

void TextFragmentHandler::RemoveFragments() {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));

  GetTextFragmentSelectorGenerator()
      ->GetFrame()
      ->View()
      ->DismissFragmentAnchor();
}

// static
bool TextFragmentHandler::IsOverTextFragment(HitTestResult result) {
  if (!result.InnerNode() || !result.InnerNodeFrame()) {
    return false;
  }

  DocumentMarkerController& marker_controller =
      result.InnerNodeFrame()->GetDocument()->Markers();
  PositionWithAffinity pos_with_affinity = result.GetPosition();
  const Position marker_position = pos_with_affinity.GetPosition();
  auto markers = marker_controller.MarkersAroundPosition(
      ToPositionInFlatTree(marker_position),
      DocumentMarker::MarkerTypes::TextFragment());
  return !markers.IsEmpty();
}

void TextFragmentHandler::ExtractTextFragmentsMatches(
    ExtractTextFragmentsMatchesCallback callback) {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));
  Vector<String> text_fragment_matches;

  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor) {
    std::move(callback).Run(Vector<String>());
    return;
  }

  for (auto& finder : anchor->TextFragmentFinders()) {
    if (finder->FirstMatch()) {
      text_fragment_matches.push_back(
          PlainText(finder->FirstMatch()->ToEphemeralRange()));
    }
  }

  std::move(callback).Run(text_fragment_matches);
}

void TextFragmentHandler::ExtractFirstFragmentRect(
    ExtractFirstFragmentRectCallback callback) {
  DCHECK(
      base::FeatureList::IsEnabled(shared_highlighting::kSharedHighlightingV2));
  IntRect rect_in_viewport;

  TextFragmentAnchor* anchor = GetTextFragmentAnchor();
  if (!anchor || anchor->TextFragmentFinders().size() <= 0) {
    std::move(callback).Run(gfx::Rect());
    return;
  }

  for (auto& finder : anchor->TextFragmentFinders()) {
    if (finder->FirstMatch() == nullptr) {
      continue;
    }

    PhysicalRect bounding_box(
        ComputeTextRect(finder->FirstMatch()->ToEphemeralRange()));
    rect_in_viewport =
        GetTextFragmentSelectorGenerator()->GetFrame()->View()->FrameToViewport(
            EnclosingIntRect(bounding_box));
    break;
  }

  std::move(callback).Run(gfx::Rect(rect_in_viewport));
}

void TextFragmentHandler::Trace(Visitor* visitor) const {
  visitor->Trace(text_fragment_selector_generator_);
  visitor->Trace(selector_producer_);
}

TextFragmentAnchor* TextFragmentHandler::GetTextFragmentAnchor() {
  FragmentAnchor* fragmentAnchor = GetTextFragmentSelectorGenerator()
                                       ->GetFrame()
                                       ->View()
                                       ->GetFragmentAnchor();
  if (!fragmentAnchor || !fragmentAnchor->IsTextFragmentAnchor()) {
    return nullptr;
  }
  return static_cast<TextFragmentAnchor*>(fragmentAnchor);
}

}  // namespace blink
