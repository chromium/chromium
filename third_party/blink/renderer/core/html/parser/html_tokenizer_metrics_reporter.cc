// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_tokenizer_metrics_reporter.h"

#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"

namespace blink {
namespace {

const LChar kCData[] = "<![CDATA[";
// Don't include the \0 in the count.
constexpr wtf_size_t kCDataLength =
    static_cast<wtf_size_t>(std::size(kCData) - 1);

}  // namespace

// static
base::RepeatingClosure*
    HTMLTokenizerMetricsReporter::metrics_logged_callback_for_test_ = nullptr;

HTMLTokenizerMetricsReporter::BackgroundReporter::~BackgroundReporter() {
  // Only log if something was actually parsed.
  if (input_length_encountered_ == 0)
    return;

  const int bitmask =
      (document_write_encountered_ ? 1 : 0) |
      (speculative_state_mismatch_ ? 2 : 0) |
      (index_of_null_char_ != std::numeric_limits<unsigned>::max() ? 4 : 0) |
      (index_of_cdata_section_ != std::numeric_limits<unsigned>::max() ? 8 : 0);
  base::UmaHistogramExactLinear("Blink.Tokenizer.MainDocument.ATypicalStates",
                                bitmask, 16);
  if (bitmask != 0) {
    const unsigned min_position = std::min(
        std::min(write_or_state_mismatch_position_, index_of_null_char_),
        index_of_cdata_section_);
    base::UmaHistogramCounts10M(
        "Blink.Tokenizer.MainDocument.LocationOfFirstATypicalState",
        min_position);
  }
  if (metrics_logged_callback_for_test_)
    metrics_logged_callback_for_test_->Run();
}

void HTMLTokenizerMetricsReporter::BackgroundReporter::WillAppend(
    const String& content) {
  UpdateIndexOfNullChar(content);
  UpdateIndexOfCDATA(content);
  input_length_encountered_ += content.length();
}

void HTMLTokenizerMetricsReporter::BackgroundReporter::DocumentWriteEncountered(
    int position) {
  DCHECK(!document_write_encountered_);
  document_write_encountered_ = true;
  write_or_state_mismatch_position_ = std::min(
      write_or_state_mismatch_position_, static_cast<unsigned>(position));
}

void HTMLTokenizerMetricsReporter::BackgroundReporter::SpeculativeStateMismatch(
    int position) {
  DCHECK(!speculative_state_mismatch_);
  speculative_state_mismatch_ = true;
  write_or_state_mismatch_position_ = std::min(
      write_or_state_mismatch_position_, static_cast<unsigned>(position));
}

void HTMLTokenizerMetricsReporter::BackgroundReporter::UpdateIndexOfNullChar(
    const String& string) {
  if (index_of_null_char_ != std::numeric_limits<unsigned>::max())
    return;

  index_of_null_char_ = string.find(static_cast<UChar>('\0'));
  if (index_of_null_char_ != kNotFound)
    index_of_null_char_ += input_length_encountered_;
  else
    index_of_null_char_ = std::numeric_limits<unsigned>::max();
}

bool HTMLTokenizerMetricsReporter::BackgroundReporter::
    MatchPossibleCDataSection(const String& string,
                              wtf_size_t start_string_index) {
  DCHECK_GT(num_matching_cdata_chars_, 0u);
  DCHECK_LT(start_string_index, kNotFound);
  const wtf_size_t string_length = string.length();
  wtf_size_t i = 0;
  // Iterate through `string` while it matches `kCData`. i starts at 0, but is
  // relative to `start_string_index`. `num_matching_cdata_chars_` is the
  // position into `kCData` to start the match from (portion of previous string
  // that matched).
  while (i + num_matching_cdata_chars_ < kCDataLength &&
         (i + start_string_index) < string_length &&
         string[i + start_string_index] ==
             kCData[i + num_matching_cdata_chars_]) {
    ++i;
  }
  if (i + num_matching_cdata_chars_ == kCDataLength) {
    // Matched all cdata.
    index_of_cdata_section_ =
        input_length_encountered_ + i + start_string_index - kCDataLength;
    return true;
  }
  if ((i + start_string_index) == string_length) {
    // This branch is hit in the case of matching all available data, but
    // more is required for a full match.
    num_matching_cdata_chars_ += i;
    return true;
  }
  return false;
}

void HTMLTokenizerMetricsReporter::BackgroundReporter::UpdateIndexOfCDATA(
    const String& string) {
  if (index_of_cdata_section_ != std::numeric_limits<unsigned>::max())
    return;

  if (num_matching_cdata_chars_ != 0) {
    if (MatchPossibleCDataSection(string, 0))
      return;
    num_matching_cdata_chars_ = 0;
  }

  for (wtf_size_t next_possible_index = 0; next_possible_index != kNotFound;
       next_possible_index = string.find(kCData[0], next_possible_index)) {
    num_matching_cdata_chars_ = 1;
    if (MatchPossibleCDataSection(string, next_possible_index + 1))
      return;
    ++next_possible_index;
  }
  num_matching_cdata_chars_ = 0;
}

HTMLTokenizerMetricsReporter::HTMLTokenizerMetricsReporter(
    const HTMLTokenizer* tokenizer)
    : tokenizer_(tokenizer),
      background_reporter_(worker_pool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT})) {}

void HTMLTokenizerMetricsReporter::WillConstructTreeFromToken(
    const AtomicHTMLToken& token,
    const SegmentedString& input) {
  if (speculative_state_mismatch_)
    return;

  if (token.GetType() == HTMLToken::kStartTag) {
    last_token_was_start_ = true;
    tokenizer_state_for_start_ =
        tokenizer_->SpeculativeStateForTag(token.GetName());
  }
}

void HTMLTokenizerMetricsReporter::WillChangeTokenizerState(
    const SegmentedString& input,
    const AtomicHTMLToken& token,
    HTMLTokenizer::State state) {
  if (speculative_state_mismatch_)
    return;

  if (token.GetType() != HTMLToken::kStartTag &&
      state != tokenizer_->GetState()) {
    RecordSpeculativeStateMismatch(input.NumberOfCharactersConsumed());
  }
}

void HTMLTokenizerMetricsReporter::OnDocumentWrite(
    const SegmentedString& input) {
  if (document_write_encountered_)
    return;
  document_write_encountered_ = true;
  // At the time this is called `input` should have a next segment string with
  // the data before the write. See InsertionPointRecord.
  const int length =
      input.NextSegmentedString()
          ? input.NextSegmentedString()->NumberOfCharactersConsumed()
          : input.NumberOfCharactersConsumed();
  background_reporter_.AsyncCall(&BackgroundReporter::DocumentWriteEncountered)
      .WithArgs(length);
}

void HTMLTokenizerMetricsReporter::WillAppend(const String& content) {
  background_reporter_.AsyncCall(&BackgroundReporter::WillAppend)
      .WithArgs(content);
}

void HTMLTokenizerMetricsReporter::RecordSpeculativeStateMismatch(
    int position) {
  DCHECK(!speculative_state_mismatch_);
  speculative_state_mismatch_ = true;
  background_reporter_.AsyncCall(&BackgroundReporter::SpeculativeStateMismatch)
      .WithArgs(position);
}

}  // namespace blink
