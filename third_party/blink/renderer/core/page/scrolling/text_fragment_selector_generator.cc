// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector_generator.h"

#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/interface_registry.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_iterator.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"

namespace blink {

constexpr int kExactTextMaxChars = 300;
constexpr int kNoContextMinChars = 20;

void TextFragmentSelectorGenerator::UpdateSelection(
    LocalFrame* selection_frame,
    const EphemeralRangeInFlatTree& selection_range) {
  DCHECK(selection_frame);

  // Scroll-to-text doesn't support iframes.
  DCHECK(selection_frame->IsMainFrame());

  selection_frame_ = selection_frame;
  selection_range_ = MakeGarbageCollected<Range>(
      selection_range.GetDocument(),
      ToPositionInDOMTree(selection_range.StartPosition()),
      ToPositionInDOMTree(selection_range.EndPosition()));
}

void TextFragmentSelectorGenerator::BindTextFragmentSelectorProducer(
    mojo::PendingReceiver<mojom::blink::TextFragmentSelectorProducer>
        producer) {
  selector_producer_.reset();
  selector_producer_.Bind(
      std::move(producer),
      selection_frame_->GetTaskRunner(blink::TaskType::kInternalDefault));
}

void TextFragmentSelectorGenerator::GenerateSelector(
    GenerateSelectorCallback callback) {
  DCHECK(selection_range_);
  DCHECK(callback);

  pending_generate_selector_callback_ = std::move(callback);
  EphemeralRangeInFlatTree ephemeral_range(selection_range_);

  const TextFragmentSelector kInvalidSelector(
      TextFragmentSelector::SelectorType::kInvalid);

  Node& start_first_block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(
          *ephemeral_range.StartPosition().AnchorNode());
  Node& end_first_block_ancestor =
      FindBuffer::GetFirstBlockLevelAncestorInclusive(
          *ephemeral_range.EndPosition().AnchorNode());

  if (!start_first_block_ancestor.isSameNode(&end_first_block_ancestor)) {
    NotifySelectorReady(kInvalidSelector);
    return;
  }

  // TODO(gayane): If same node, need to check if start and end are interrupted
  // by a block. Example: <div>start of the selection <div> sub block </div>end
  // of the selection</div>.

  // TODO(gayane): Move selection start and end to contain full words.

  String selected_text = PlainText(ephemeral_range);

  if (selected_text.length() < kNoContextMinChars ||
      selected_text.length() > kExactTextMaxChars) {
    NotifySelectorReady(kInvalidSelector);
    return;
  }

  selector_ = std::make_unique<TextFragmentSelector>(
      TextFragmentSelector::SelectorType::kExact, selected_text, "", "", "");
  TextFragmentFinder finder(*this, *selector_);
  finder.FindMatch(*selection_frame_->GetDocument());
}

void TextFragmentSelectorGenerator::DidFindMatch(
    const EphemeralRangeInFlatTree& match,
    const TextFragmentAnchorMetrics::Match match_metrics,
    bool is_unique) {
  if (is_unique) {
    NotifySelectorReady(*selector_);
  } else {
    // TODO(gayane): Should add more range and/or context.
    NotifySelectorReady(
        TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid));
  }
}

void TextFragmentSelectorGenerator::NotifySelectorReady(
    const TextFragmentSelector& selector) {
  DCHECK(pending_generate_selector_callback_);
  std::move(pending_generate_selector_callback_).Run(selector.ToString());
}

void TextFragmentSelectorGenerator::ClearSelection() {
  if (selection_range_) {
    selection_range_->Dispose();
    selection_range_ = nullptr;
    selection_frame_ = nullptr;
  }
}

void TextFragmentSelectorGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(selection_frame_);
  visitor->Trace(selection_range_);
  visitor->Trace(selector_producer_);
}

}  // namespace blink
