// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/annotation/text_annotation_selector.h"

namespace blink {

TextAnnotationSelector::TextAnnotationSelector(
    const TextFragmentSelector& params)
    : params_(params) {}

void TextAnnotationSelector::Trace(Visitor* visitor) const {
  visitor->Trace(finder_);
  AnnotationSelector::Trace(visitor);
}

String TextAnnotationSelector::Serialize() const {
  return params_.ToString();
}

void TextAnnotationSelector::FindRange(Document& document,
                                       SearchType type,
                                       FinishedCallback finished_cb) {
  TextFragmentFinder::FindBufferRunnerType find_buffer_type;
  switch (type) {
    case kSynchronous:
      find_buffer_type = TextFragmentFinder::kSynchronous;
      break;
    case kAsynchronous:
      find_buffer_type = TextFragmentFinder::kAsynchronous;
      break;
  }

  was_unique_.reset();

  finder_ = MakeGarbageCollected<TextFragmentFinder>(*this, params_, &document,
                                                     find_buffer_type);
  finished_callback_ = std::move(finished_cb);
  finder_->FindMatch();
}

void TextAnnotationSelector::DidFindMatch(const RangeInFlatTree& range,
                                          bool is_unique) {
  was_unique_ = is_unique;

  DCHECK(finished_callback_);
  std::move(finished_callback_).Run(&range);

  finder_.Clear();
}

void TextAnnotationSelector::NoMatchFound() {
  DCHECK(finished_callback_);
  std::move(finished_callback_).Run(nullptr);
  finder_.Clear();
}

bool TextAnnotationSelector::WasMatchUnique() const {
  DCHECK(was_unique_.has_value());
  return *was_unique_;
}

}  // namespace blink
