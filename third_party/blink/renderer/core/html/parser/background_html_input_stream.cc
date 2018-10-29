/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GOOGLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/html/parser/background_html_input_stream.h"

namespace blink {

BackgroundHTMLInputStream::BackgroundHTMLInputStream()
    : first_valid_checkpoint_index_(0),
      first_valid_segment_index_(0),
      total_checkpoint_token_count_(0) {}

void BackgroundHTMLInputStream::Append(const String& input) {
  current_.Append(SegmentedString(input));
  segments_.push_back(input);
}

void BackgroundHTMLInputStream::Close() {
  current_.Close();
}

HTMLInputCheckpoint BackgroundHTMLInputStream::CreateCheckpoint(
    wtf_size_t tokens_extracted_since_previous_checkpoint) {
  HTMLInputCheckpoint checkpoint = checkpoints_.size();
  checkpoints_.push_back(Checkpoint(
      current_, segments_.size(), tokens_extracted_since_previous_checkpoint));
  total_checkpoint_token_count_ += tokens_extracted_since_previous_checkpoint;
  return checkpoint;
}

void BackgroundHTMLInputStream::InvalidateCheckpointsBefore(
    HTMLInputCheckpoint new_first_valid_checkpoint_index) {
  DCHECK_LT(new_first_valid_checkpoint_index, checkpoints_.size());
  // There is nothing to do for the first valid checkpoint.
  if (first_valid_checkpoint_index_ == new_first_valid_checkpoint_index)
    return;

  DCHECK_GT(new_first_valid_checkpoint_index, first_valid_checkpoint_index_);
  const Checkpoint& last_invalid_checkpoint =
      checkpoints_[new_first_valid_checkpoint_index - 1];

  DCHECK_LE(first_valid_segment_index_,
            last_invalid_checkpoint.number_of_segments_already_appended);
  for (wtf_size_t i = first_valid_segment_index_;
       i < last_invalid_checkpoint.number_of_segments_already_appended; ++i)
    segments_[i] = String();
  first_valid_segment_index_ =
      last_invalid_checkpoint.number_of_segments_already_appended;

  for (wtf_size_t i = first_valid_checkpoint_index_;
       i < new_first_valid_checkpoint_index; ++i)
    checkpoints_[i].Clear();
  first_valid_checkpoint_index_ = new_first_valid_checkpoint_index;

  UpdateTotalCheckpointTokenCount();
}

void BackgroundHTMLInputStream::RewindTo(HTMLInputCheckpoint checkpoint_index,
                                         const String& unparsed_input) {
  DCHECK_LT(checkpoint_index,
            checkpoints_
                .size());  // If this DCHECK fires, checkpointIndex is invalid.
  const Checkpoint& checkpoint = checkpoints_[checkpoint_index];

#if DCHECK_IS_ON()
  DCHECK(!checkpoint.IsNull());
#endif

  bool is_closed = current_.IsClosed();

  current_ = checkpoint.input;

  for (wtf_size_t i = checkpoint.number_of_segments_already_appended;
       i < segments_.size(); ++i) {
    DCHECK(!segments_[i].IsNull());
    current_.Append(SegmentedString(segments_[i]));
  }

  if (!unparsed_input.IsEmpty()) {
    current_.Prepend(SegmentedString(unparsed_input),
                     SegmentedString::PrependType::kNewInput);
  }

  if (is_closed && !current_.IsClosed())
    current_.Close();

  DCHECK_EQ(current_.IsClosed(), is_closed);

  segments_.clear();
  checkpoints_.clear();
  first_valid_checkpoint_index_ = 0;
  first_valid_segment_index_ = 0;

  UpdateTotalCheckpointTokenCount();
}

void BackgroundHTMLInputStream::UpdateTotalCheckpointTokenCount() {
  total_checkpoint_token_count_ = 0;
  for (const auto& checkpoint : checkpoints_) {
    total_checkpoint_token_count_ +=
        checkpoint.tokens_extracted_since_previous_checkpoint;
  }
}

}  // namespace blink
