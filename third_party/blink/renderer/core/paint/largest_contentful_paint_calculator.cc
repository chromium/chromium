// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/largest_contentful_paint_calculator.h"

#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/inspector/identifiers_factory.h"
#include "third_party/blink/renderer/core/paint/image_element_timing.h"

namespace blink {

namespace {

constexpr const char kTraceCategories[] = "loading,rail,devtools.timeline";

constexpr const char kLCPCandidate[] = "largestContentfulPaint::Candidate";

}  // namespace

LargestContentfulPaintCalculator::LargestContentfulPaintCalculator(
    WindowPerformance* window_performance)
    : window_performance_(window_performance) {}

void LargestContentfulPaintCalculator::UpdateLargestContentPaintIfNeeded(
    base::WeakPtr<TextRecord> largest_text,
    const ImageRecord* largest_image) {
  uint64_t text_size = largest_text ? largest_text->first_size : 0u;
  uint64_t image_size = largest_image ? largest_image->first_size : 0u;
  if (image_size > text_size) {
    if (image_size > largest_reported_size_ &&
        largest_image->paint_time > base::TimeTicks()) {
      UpdateLargestContentfulImage(largest_image);
    }
  } else {
    if (text_size > largest_reported_size_ &&
        largest_text->paint_time > base::TimeTicks()) {
      UpdateLargestContentfulText(largest_text);
    }
  }
}

void LargestContentfulPaintCalculator::UpdateLargestContentfulImage(
    const ImageRecord* largest_image) {
  DCHECK(window_performance_);
  DCHECK(largest_image);
  const ImageResourceContent* cached_image = largest_image->cached_image;
  Node* image_node = DOMNodeIds::NodeForId(largest_image->node_id);

  // |cached_image| is a weak pointer, so it may be null. This can only happen
  // if the image has been removed, which means that the largest image is not
  // up-to-date. This can happen when this method call came from
  // OnLargestTextUpdated(). If a largest-image is added and removed so fast
  // that it does not get to be reported here, we consider it safe to ignore.
  // For similar reasons, |image_node| may be null and it is safe to ignore
  // the |largest_image| content in this case as well.
  if (!cached_image || !image_node)
    return;

  largest_reported_size_ = largest_image->first_size;
  const KURL& url = cached_image->Url();
  bool expose_paint_time_to_api =
      url.ProtocolIsData() || cached_image->GetResponse().TimingAllowPassed();
  const String& image_url =
      url.ProtocolIsData()
          ? url.GetString().Left(ImageElementTiming::kInlineImageMaxChars)
          : url.GetString();
  // Do not expose element attribution from shadow trees.
  Element* image_element =
      image_node->IsInShadowTree() ? nullptr : To<Element>(image_node);
  const AtomicString& image_id =
      image_element ? image_element->GetIdAttribute() : AtomicString();
  window_performance_->OnLargestContentfulPaintUpdated(
      expose_paint_time_to_api ? largest_image->paint_time : base::TimeTicks(),
      largest_image->first_size, largest_image->load_time, image_id, image_url,
      image_element);

  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(kTraceCategories, kLCPCandidate,
                                     largest_image->paint_time, "data",
                                     ImageCandidateTraceData(largest_image),
                                     "frame", ToTraceValue(window->GetFrame()));
  }
}

void LargestContentfulPaintCalculator::UpdateLargestContentfulText(
    base::WeakPtr<TextRecord> largest_text) {
  DCHECK(window_performance_);
  DCHECK(largest_text);
  Node* text_node = DOMNodeIds::NodeForId(largest_text->node_id);
  // |text_node| could be null and |largest_text| should be ignored in this
  // case. This can happen when the largest-text gets removed too fast and does
  // not get to be reported here.
  if (!text_node)
    return;

  largest_reported_size_ = largest_text->first_size;
  // Do not expose element attribution from shadow trees.
  Element* text_element =
      text_node->IsInShadowTree() ? nullptr : To<Element>(text_node);
  const AtomicString& text_id =
      text_element ? text_element->GetIdAttribute() : AtomicString();
  window_performance_->OnLargestContentfulPaintUpdated(
      largest_text->paint_time, largest_text->first_size, base::TimeTicks(),
      text_id, g_empty_string, text_element);

  if (LocalDOMWindow* window = window_performance_->DomWindow()) {
    TRACE_EVENT_MARK_WITH_TIMESTAMP2(kTraceCategories, kLCPCandidate,
                                     largest_text->paint_time, "data",
                                     TextCandidateTraceData(largest_text),
                                     "frame", ToTraceValue(window->GetFrame()));
  }
}

void LargestContentfulPaintCalculator::Trace(Visitor* visitor) const {
  visitor->Trace(window_performance_);
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::TextCandidateTraceData(
    base::WeakPtr<TextRecord> largest_text) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", "text");
  value->SetInteger("nodeId", static_cast<int>(largest_text->node_id));
  value->SetInteger("size", static_cast<int>(largest_text->first_size));
  value->SetInteger("candidateIndex", ++count_candidates_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  value->SetString("navigationId",
                   IdentifiersFactory::LoaderId(window->document()->Loader()));
  return value;
}

std::unique_ptr<TracedValue>
LargestContentfulPaintCalculator::ImageCandidateTraceData(
    const ImageRecord* largest_image) {
  auto value = std::make_unique<TracedValue>();
  value->SetString("type", "image");
  value->SetInteger("nodeId", static_cast<int>(largest_image->node_id));
  value->SetInteger("size", static_cast<int>(largest_image->first_size));
  value->SetInteger("candidateIndex", ++count_candidates_);
  auto* window = window_performance_->DomWindow();
  value->SetBoolean("isMainFrame", window->GetFrame()->IsMainFrame());
  value->SetString("navigationId",
                   IdentifiersFactory::LoaderId(window->document()->Loader()));

  return value;
}

}  // namespace blink
