// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKENIZER_METRICS_REPORTER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKENIZER_METRICS_REPORTER_H_

#include <limits>

#include "base/callback.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/sequence_bound.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace blink {

class AtomicHTMLToken;
class SegmentedString;

// HTMLTokenizerMetricsReporter is used to track how often a handful of
// non-typical cases occur when tokenizing. It specifically tracks the
// following:
// . document.write().
// . A null character.
// . CDATA section.
// . How often SpeculativeStateForTag() doesn't match the actual state.
// This code is called on the critical path, so detection of a null character
// and CDATA are done in the background. Logging is done once in the destructor
// (in the background).
//
// TODO(crbug.com/1345267): remove this class once data has been collected.
class CORE_EXPORT HTMLTokenizerMetricsReporter {
  USING_FAST_MALLOC(HTMLTokenizerMetricsReporter);

 public:
  explicit HTMLTokenizerMetricsReporter(const HTMLTokenizer* tokenizer);

  // Called prior to HTMLTokenizer::NextToken().
  void WillProcessNextToken(const SegmentedString& input) {
    if (speculative_state_mismatch_)
      return;

    if (last_token_was_start_) {
      last_token_was_start_ = false;
      if (tokenizer_state_for_start_.has_value() &&
          tokenizer_state_for_start_ != tokenizer_->GetState()) {
        RecordSpeculativeStateMismatch(input.NumberOfCharactersConsumed());
      }
    }
  }

  // Called after a token has been created by the tokenizer but before
  // ConstructTree().
  void WillConstructTreeFromToken(const AtomicHTMLToken& token,
                                  const SegmentedString& input);

  // Called when the state of the tokenizer is going to be explicitly set.
  void WillChangeTokenizerState(const SegmentedString& input,
                                const AtomicHTMLToken& token,
                                HTMLTokenizer::State state);

  // Called when document.write() occurs.
  void OnDocumentWrite(const SegmentedString& input);

  // Called when data is available to be tokenized.
  void WillAppend(const String& content);

  // BackgroundReporter does the actual metric recording, as well as any
  // non-trivial processing. The public methods of HTMLTokenizerMetricsReporter
  // call through to this object so that it can log the metrics in the
  // destructor.
  //
  // NOTE: public for tests, treat as private.
  class CORE_EXPORT BackgroundReporter {
    USING_FAST_MALLOC(BackgroundReporter);

   public:
    ~BackgroundReporter();

    void WillAppend(const String& content);
    void DocumentWriteEncountered(int position);
    void SpeculativeStateMismatch(int position);

    unsigned index_of_null_char() const { return index_of_null_char_; }

    unsigned index_of_cdata_section() const { return index_of_cdata_section_; }

   private:
    void UpdateIndexOfNullChar(const String& string);
    // Attempts to match a possible cdata secition. `string` is the string to
    // search in, starting at `start_string_index`. Returns true on success,
    // or the section matches but the end of input is reached (partial match).
    bool MatchPossibleCDataSection(const String& string,
                                   wtf_size_t start_string_index);
    void UpdateIndexOfCDATA(const String& string);

    bool document_write_encountered_ = false;
    bool speculative_state_mismatch_ = false;
    unsigned write_or_state_mismatch_position_ =
        std::numeric_limits<unsigned>::max();

    // Amount of data encountered to date (sum of length of strings supplied
    // to WillAppend()).
    unsigned input_length_encountered_ = 0;

    unsigned index_of_null_char_ = std::numeric_limits<unsigned>::max();

    // Following is used to match a CDATA section:
    unsigned index_of_cdata_section_ = std::numeric_limits<unsigned>::max();
    unsigned num_matching_cdata_chars_ = 0;
  };

  // Run when metrics have been logged. Provided for tests. This callback is
  // run on a background thread.
  static base::RepeatingClosure* metrics_logged_callback_for_test_;

 private:
  void RecordSpeculativeStateMismatch(int position);

  const HTMLTokenizer* tokenizer_;

  // True if the last token was a start.
  bool last_token_was_start_ = false;

  // If the last token was a start tag, this is the corresponding speculative
  // state (which may not be set).
  absl::optional<HTMLTokenizer::State> tokenizer_state_for_start_;

  // Whether document.write() was encountered.
  bool document_write_encountered_ = false;

  // Whether the Tokenizer state from the builder does not match the
  // speculative state.
  bool speculative_state_mismatch_ = false;

  WTF::SequenceBound<BackgroundReporter> background_reporter_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKENIZER_METRICS_REPORTER_H_
