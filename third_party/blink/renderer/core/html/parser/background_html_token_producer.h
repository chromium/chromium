// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_BACKGROUND_HTML_TOKEN_PRODUCER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_BACKGROUND_HTML_TOKEN_PRODUCER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html/parser/special_sequences_tracker.h"
#include "third_party/blink/renderer/platform/text/segmented_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "third_party/blink/renderer/platform/wtf/wtf_size_t.h"

namespace base {
class SequencedTaskRunner;
}

namespace WTF {
class String;
}

namespace blink {

struct HTMLTokenizerSnapshot;

// A single result from the background producer:
// . A valid HTMLToken, in which case `token` is non-null.
// . A state was encountered the background producer can not handle (because it
//   needs state from the tree builder). In this case `token` is null and
//   `tokenizer_snapshot` contains the snapshot from the last token.
struct BackgroundHTMLTokenProducerParseResult {
  USING_FAST_MALLOC(BackgroundHTMLTokenProducerParseResult);

 public:
  // See class description for details.
  std::unique_ptr<HTMLToken> token;

  // Captures state from SegmentedString (see SegmentedString for details on
  // these). These are only set if `token` is non-null.
  unsigned num_chars_processed;
  unsigned num_lines_processed;
  int column_position_at_end;

  // Captures the state of the tokenizer after the token was produced.
  HTMLTokenizerSnapshot tokenizer_snapshot;

  // True if the tokenizer state was changed because SpeculativeStateForTag()
  // returned a value that differs from the HTMLTokenizer's state.
  bool was_tokenizer_state_change_speculative;

  // If `was_tokenizer_state_change_speculative` is set, this gives the original
  // state of the tokenizer.
  HTMLTokenizer::State state_before_speculative_state_change;
};

// Captures why background token production was stopped.
enum class BackgroundHTMLTokenProducerShutdownReason {
  // The end was reached.
  kDone,

  // document.write() was called.
  kDocumentWrite,

  // The state from the tree builder did not match the speculative state.
  kStateMismatch,

  // A sequence was encountered that requires state that only the tree builder
  // knows.
  kSpecialSequence,
};

// Class responsible for generating HTMLTokens in a background thread. Token
// production is done as soon as content is available. Internally this class
// keeps two vectors with the token results. As tokens are produced they are
// added to one vector, and another vector with the current results the main
// is using. A limited number of tokens are produced at a time. The expected
// use case is to add content (AppendToEnd()) and call NextParseResults() to
// get the parsed results. NextParseResults() always returns the next set of
// tokens until the end of content is reached, in which case null is returned.
//
// As this class runs on two threads, the destructor is private and deletion
// only happens once ShutdownAndScheduleDeletion() is called.
class CORE_EXPORT BackgroundHTMLTokenProducer {
  USING_FAST_MALLOC(BackgroundHTMLTokenProducer);

 public:
  BackgroundHTMLTokenProducer(
      const SegmentedString& input,
      std::unique_ptr<HTMLTokenizer> tokenizer,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  BackgroundHTMLTokenProducer(const BackgroundHTMLTokenProducer&) = delete;
  BackgroundHTMLTokenProducer& operator=(const BackgroundHTMLTokenProducer&) =
      delete;

  using Results = WTF::Vector<BackgroundHTMLTokenProducerParseResult>;

  // Adds new content to parse. This signals the background thread to start
  // parsing.
  void AppendToEnd(const String& string);

  // Signals no more data will be available.
  void MarkEndOfFile();

  // Schedules shutdown and deletes this (deletion happens on the background
  // thread).
  void ShutdownAndScheduleDeletion(
      BackgroundHTMLTokenProducerShutdownReason reason);

  // Returns the next set of results. This returns null if the end of input has
  // been reached, otherwise it returns a non-null value of the current results
  // that have been produced. The return value is owned by this class, and valid
  // until NextParseResults() is called and returns a non-null value.
  //
  // Note that this internally blocks until tokens have been produced.
  Results* NextParseResults();

 private:
  // Results of calling ApplyDataFromMainThread().
  struct ApplyDataResult {
    STACK_ALLOCATED();

   public:
    // `input_generation_` at the time ApplyDataFromMainThread() was called.
    uint8_t input_generation = 0;

    // True if ShutdownAndScheduleDeletion() was called.
    bool stop_and_delete_this = false;
  };

  ~BackgroundHTMLTokenProducer();

  // Responsible for generating tokens until ShutdownAndScheduleDeletion() is
  // called.
  void RunTokenizeLoopOnTaskRunner();

  // Deletes this on the background thread. Called as a result of
  // ShutdownAndScheduleDeletion().
  void DeleteOnTaskRunner();

  // Applies data from the main thread to the background thread.
  ApplyDataResult ApplyDataFromMainThread();

  // Adds a token result and notifies the main thread data is available.
  void AppendTokenResult(
      std::unique_ptr<HTMLToken> token,
      bool was_tokenizer_state_change_speculative,
      HTMLTokenizer::State state_before_speculative_state_change);

  // Notifies the main thread the end of input has been reached.
  void NotifyEndOfInput(uint8_t input_generation);

  // Adds a result indicating an unhandled sequence was encountered.
  void AppendUnhandledSequenceResult();

  // Called internally to add a result and notify the main thread.
  void AppendResultAndNotify(
      std::unique_ptr<HTMLToken> token,
      unsigned num_chars_processed,
      unsigned num_lines_processed,
      int column_position_at_end,
      bool was_tokenizer_state_change_speculative,
      HTMLTokenizer::State state_before_speculative_state_change);

  void UpdateHistogramRelatedTotals() EXCLUSIVE_LOCKS_REQUIRED(&results_lock_);

  bool IsRunningOnMainThread() const;

  bool IsRunningOnBackgroundTaskRunner() const;

  // This lock is generally used for data that flows from the main thread to the
  // background.
  base::Lock input_lock_;

  // Signals data from the main thread is available to the background thread.
  base::ConditionVariable data_available_{&input_lock_};

  // Data from AppendToEnd() is added here. The background thread adds these
  // to `input_` (which is used by the tokenizer).
  WTF::Vector<String> strings_to_append_ GUARDED_BY(input_lock_);

  // Set once MarkEndOfFile() is called
  bool end_of_file_ GUARDED_BY(input_lock_) = false;

  // Set once ShutdownAndScheduleDeletion() is called.
  bool stop_and_delete_ GUARDED_BY(input_lock_) = false;

  // A value that increases every time one of AppendToEnd() or MarkEndOfFile()
  // is called. This is used to detect when the end of input has been reached.
  // This is modified by the main thread, and read by the background thread.
  //
  // Mutation is guarded by `input_lock_` but as it's also read from
  // NextParseResults(), which is on the critical path, it does not have the
  // GUARDED_BY annotation.
  uint8_t input_generation_ = 0;

  // `tokenizer_` operates on this.
  SegmentedString input_;

  std::unique_ptr<HTMLTokenizer> tokenizer_;

  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // Lock used to guard data generated from the background thread.
  base::Lock results_lock_;

  // The two vectors of results maintained by this class. The background thread
  // adds to `bg_thread_results_`. When NextParseResults() is called the main
  // thread swaps the two vectors and sets `clear_results_before_next_append_`.
  // The next time the background produces a result it clears
  // `bg_thread_results_`. Clearing is done on the background thread as it will
  // free up a bunch of memory, which is expensive and should be avoided on the
  // main thread.
  Results main_thread_results_ GUARDED_BY(results_lock_);
  Results bg_thread_results_ GUARDED_BY(results_lock_);
  bool clear_results_before_next_append_ GUARDED_BY(results_lock_) = false;

  // Updated by the background thread once it has finished processing input
  // for the corresponding `input_generation_`.
  uint8_t processed_input_generation_ GUARDED_BY(results_lock_) = 0;

  // Signals the main thread once results are available.
  base::ConditionVariable results_available_{&results_lock_};

  // Size of `bg_thread_results_` when end of input was reached. Used for
  // metrics.
  wtf_size_t end_of_input_bg_thread_result_size_ = kNotFound;

  // Signals the background thread that `clear_results_before_next_append_`
  // was set.
  base::ConditionVariable clear_results_was_set_{&results_lock_};

  // Tracks state for the current token.
  struct InProgressTokenData {
    // Length of `input_` when processing started.
    wtf_size_t string_length_at_start_of_token = 0;

    // Current line of `input_` when processing started.
    int line_count_at_start_of_token = 0;
  };

  // Tracks the state of the current token being processed. State is captured
  // prior to calling Tokenizer::NextToken() and persists if NextToken()
  // returns null.
  absl::optional<InProgressTokenData> in_progress_token_data_;

  // Tracks sequences that may not be handled correctly by this class.
  SpecialSequencesTracker special_sequences_tracker_;

  // The token count and number of calls to NextParseResults() are tracked in
  // two set of members. This is done to differentiate between when parsing
  // stopped because of end of input vs when more input is available. The two
  // are tracked differently so that separate histograms can be recorded.

  // Total number of tokens processed and how many calls to NextParseResults()
  // it took. This is incremented after NextParseResults(), and only if the
  // chunks did not contain the last input.
  unsigned num_calls_to_next_parse_results_ = 0;
  unsigned total_tokens_processed_ = 0;

  // Number of calls to NextParseResults() and total token count when end of
  // input was encountered.
  unsigned num_calls_to_next_parse_results_when_end_reached_ = 0;
  unsigned total_tokens_processed_when_end_reached_ = 0;

  // Reason supplied to ShutdownAndScheduleDeletion().
  absl::optional<BackgroundHTMLTokenProducerShutdownReason> shutdown_reason_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_BACKGROUND_HTML_TOKEN_PRODUCER_H_
