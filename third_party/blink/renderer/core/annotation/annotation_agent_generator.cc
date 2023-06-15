// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/annotation_agent_generator.h"

#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/editing/visible_selection.h"
#include "third_party/blink/renderer/core/fragment_directive/text_fragment_selector.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"

namespace blink {

AnnotationAgentGenerator::AnnotationAgentGenerator(LocalFrame* frame)
    : frame_(frame) {}

void AnnotationAgentGenerator::Trace(Visitor* visitor) const {
  visitor->Trace(generator_);
  visitor->Trace(frame_);
}

void AnnotationAgentGenerator::GetForCurrentSelection(
    mojom::blink::AnnotationType type,
    SelectorGenerationCallback callback) {
  // A valid callback, means there's an ongoing previous request. In that case,
  // the previous request is canceled with an error for the new one.
  // TODO(crbug.com/1313967): Find right behavior from a product perspective.
  if (callback_) {
    std::move(callback_).Run(
        type_,
        shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady,
        "", TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid),
        shared_highlighting::LinkGenerationError::kUnknown);
  }

  callback_ = std::move(callback);
  type_ = type;

  // Preemptive generation was completed.
  if (generation_result_.has_value()) {
    InvokeCompletionCallbackIfNeeded(
        shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
    return;
  }

  // If a generator already exists, it is because a search is in progress so
  // we'll return the result when it finishes.
  if (generator_)
    return;

  GenerateSelector();
}

void AnnotationAgentGenerator::InvokeCompletionCallbackIfNeeded(
    shared_highlighting::LinkGenerationReadyStatus ready_status) {
  DCHECK(callback_);
  if (selector_error_ == shared_highlighting::LinkGenerationError::kNone) {
    DCHECK(generation_result_.has_value());
    DCHECK(generator_);
  }

  std::move(callback_).Run(
      type_, ready_status,
      generator_ ? generator_->GetSelectorTargetText() : "",
      generation_result_.value(), selector_error_);

  if (generator_) {
    generator_->Reset();
  }
  generation_result_.reset();
  selector_error_ = shared_highlighting::LinkGenerationError::kNone;
}

void AnnotationAgentGenerator::PreemptivelyGenerateForCurrentSelection() {
  // A valid callback means that a generation started and the callback is
  // waiting on the result.
  if (callback_) {
    DCHECK(generator_);
    return;
  }

  // Reset generation_result if it has a value and no callback. This means that
  // preemptive link generation was triggered previously but the result was
  // never used.
  if (generation_result_.has_value()) {
    generation_result_.reset();
  }

  GenerateSelector();
}

void AnnotationAgentGenerator::GenerateSelector() {
  DCHECK(!generation_result_);

  selector_error_ = shared_highlighting::LinkGenerationError::kNone;

  VisibleSelectionInFlatTree selection =
      frame_->Selection().ComputeVisibleSelectionInFlatTree();
  if (selection.IsNone() || !selection.IsRange()) {
    if (callback_) {
      generation_result_.emplace(
          TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid));
      selector_error_ =
          shared_highlighting::LinkGenerationError::kEmptySelection;
      InvokeCompletionCallbackIfNeeded(
          shared_highlighting::LinkGenerationReadyStatus::
              kRequestedBeforeReady);
    }
    return;
  }

  EphemeralRangeInFlatTree selection_range(selection.Start(), selection.End());
  if (selection_range.IsNull() || selection_range.IsCollapsed()) {
    if (callback_) {
      generation_result_.emplace(
          TextFragmentSelector(TextFragmentSelector::SelectorType::kInvalid));
      selector_error_ = shared_highlighting::LinkGenerationError::kNoRange;
      InvokeCompletionCallbackIfNeeded(
          shared_highlighting::LinkGenerationReadyStatus::
              kRequestedBeforeReady);
    }
    return;
  }

  RangeInFlatTree* current_selection_range =
      MakeGarbageCollected<RangeInFlatTree>(selection_range.StartPosition(),
                                            selection_range.EndPosition());

  // Make sure the generator is valid before starting the generation.
  if (!generator_) {
    generator_ = MakeGarbageCollected<TextFragmentSelectorGenerator>(frame_);
  }

  generator_->Generate(
      *current_selection_range,
      WTF::BindOnce(&AnnotationAgentGenerator::DidFinishGeneration,
                    WrapWeakPersistent(this)));
}

void AnnotationAgentGenerator::DidFinishGeneration(
    const TextFragmentSelector& selector,
    shared_highlighting::LinkGenerationError error) {
  DCHECK(!generation_result_.has_value());

  generation_result_.emplace(selector);
  selector_error_ = error;

  if (callback_) {
    generation_result_.emplace(selector);
    selector_error_ = error;
    InvokeCompletionCallbackIfNeeded(
        shared_highlighting::LinkGenerationReadyStatus::kRequestedAfterReady);
  }
}

}  // namespace blink
