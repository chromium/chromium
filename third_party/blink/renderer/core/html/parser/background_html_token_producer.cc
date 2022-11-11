// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/background_html_token_producer.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html_element_lookup_trie.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
namespace {

// Cached value of `kThreadedHtmlTokenizerTokenMaxCount`.
wtf_size_t g_max_tokens = 0;

}  // namespace

BackgroundHTMLTokenProducer::BackgroundHTMLTokenProducer(
    const SegmentedString& input,
    std::unique_ptr<HTMLTokenizer> tokenizer,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : input_(input),
      tokenizer_(std::move(tokenizer)),
      task_runner_(std::move(task_runner)) {
  if (g_max_tokens == 0)
    g_max_tokens = features::kThreadedHtmlTokenizerTokenMaxCount.Get();
  // `g_max_tokens` must be > 0, otherwise no tokens will be added and this
  // code is likely to get stuck.
  DCHECK_GT(g_max_tokens, 0u);
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(
          &BackgroundHTMLTokenProducer::RunTokenizeLoopOnTaskRunner,
          CrossThreadUnretained(this)));
}

BackgroundHTMLTokenProducer::~BackgroundHTMLTokenProducer() {
  DCHECK(IsRunningOnBackgroundTaskRunner());

  // `tokenizer_` keeps a reference to `token_`, delete `tokenizer_` first to
  // ensure `token_` is still valid.
  tokenizer_.reset();

  {
    base::AutoLock auto_lock(results_lock_);
    if (input_generation_ == processed_input_generation_) {
      // When the end of input is reached the background thread doesn't call
      // UpdateHistogramRelatedTotals() (because no tokens are generated).
      UpdateHistogramRelatedTotals();
    }
  }

  // Empty documents may generate a single token, don't log in this case.
  if (total_tokens_processed_ + total_tokens_processed_when_end_reached_ > 1) {
    if (num_calls_to_next_parse_results_ > 0) {
      base::UmaHistogramCounts10000(
          "Blink.BackgroundTokenizer.AverageTokensAvailablePerCall",
          total_tokens_processed_ / num_calls_to_next_parse_results_);
    }
    if (num_calls_to_next_parse_results_when_end_reached_ > 0) {
      base::UmaHistogramCounts10000(
          "Blink.BackgroundTokenizer."
          "AverageTokensAvailablePerCallWhenEndOfInputReached",
          total_tokens_processed_when_end_reached_ /
              num_calls_to_next_parse_results_when_end_reached_);
    }
    // This class should only be deleted when ShutdownAndScheduleDeletion()
    // is called, which sets `shutdown_reason_`.
    DCHECK(shutdown_reason_.has_value());
    base::UmaHistogramBoolean(
        "Blink.BackgroundTokenizer.DidCompleteSuccessfully",
        shutdown_reason_ == BackgroundHTMLTokenProducerShutdownReason::kDone);
  }
}

void BackgroundHTMLTokenProducer::AppendToEnd(const String& string) {
  DCHECK(IsRunningOnMainThread());
  {
    base::AutoLock auto_lock(input_lock_);
    strings_to_append_.push_back(string);
    ++input_generation_;
    data_available_.Signal();
  }
}

void BackgroundHTMLTokenProducer::MarkEndOfFile() {
  DCHECK(IsRunningOnMainThread());
  {
    base::AutoLock auto_lock(input_lock_);
    end_of_file_ = true;
    ++input_generation_;
    data_available_.Signal();
  }
}

void BackgroundHTMLTokenProducer::ShutdownAndScheduleDeletion(
    BackgroundHTMLTokenProducerShutdownReason reason) {
  DCHECK(IsRunningOnMainThread());
  {
    base::AutoLock auto_lock(input_lock_);
    stop_and_delete_ = true;
    shutdown_reason_ = reason;
    data_available_.Signal();
  }

  // Also signal `clear_results_before_next_append_` for the scenario of
  // background thread waiting for main thread to consume results.
  {
    base::AutoLock auto_lock(results_lock_);
    clear_results_before_next_append_ = true;
    clear_results_was_set_.Signal();
  }

  // Use a post task for deletion to ensure background task runner doesn't
  // delete this part way through.
  PostCrossThreadTask(
      *task_runner_, FROM_HERE,
      CrossThreadBindOnce(&BackgroundHTMLTokenProducer::DeleteOnTaskRunner,
                          CrossThreadUnretained(this)));
}

BackgroundHTMLTokenProducer::Results*
BackgroundHTMLTokenProducer::NextParseResults() {
  DCHECK(IsRunningOnMainThread());
  base::AutoLock auto_lock(results_lock_);
  // If `clear_results_before_next_append_` is true, the background thread
  // hasn't yet cleared the results, so need to continue waiting.
  while (bg_thread_results_.empty() || clear_results_before_next_append_) {
    if (input_generation_ == processed_input_generation_) {
      // Background thread finished parsing all the data, no more results
      // will be produced until more data is available.
      return nullptr;
    }
    results_available_.Wait();
  }
  // The while loop above blocks until at least one result, and there shouldn't
  // be more than `g_max_tokens`.
  DCHECK(!bg_thread_results_.empty() &&
         bg_thread_results_.size() <= g_max_tokens);
  // Swap the two buffers. The background thread will clear the vector before
  // adding any new results. Using swap and not clearing the vector enables
  // destruction to happen on the background thread, this ensures the main
  // thread does not do costly destruction.
  main_thread_results_.swap(bg_thread_results_);
  clear_results_before_next_append_ = true;
  // The background thread blocks when it has processed `g_max_tokens`, signal
  // to unblock it.
  if (main_thread_results_.size() == g_max_tokens)
    clear_results_was_set_.Signal();
  return &main_thread_results_;
}

void BackgroundHTMLTokenProducer::RunTokenizeLoopOnTaskRunner() {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  while (true) {
    // Always try to apply data from the main thread.
    const ApplyDataResult apply_result = ApplyDataFromMainThread();
    if (apply_result.stop_and_delete_this)
      return;

    if (!in_progress_token_data_.has_value()) {
      in_progress_token_data_.emplace(InProgressTokenData{
          .string_length_at_start_of_token = input_.length(),
          .line_count_at_start_of_token = input_.CurrentLine().ZeroBasedInt()});
    }
    HTMLToken* token = tokenizer_->NextToken(input_);
    if (!token) {
      // Let main thread know reached end of current input.
      NotifyEndOfInput(apply_result.input_generation);

      // Wait for more data.
      base::AutoLock auto_lock(input_lock_);
      while (!end_of_file_ && !stop_and_delete_ && strings_to_append_.empty()) {
        data_available_.Wait();
      }
    } else {
      // A token was generated.
      if (static_cast<unsigned>(input_.NumberOfCharactersConsumed()) >=
          special_sequences_tracker_.index_of_first_special_sequence()) {
        // The token spans a sequence that depends upon state from the main
        // thread. Notify the main thread of this, and then exit.
        AppendUnhandledSequenceResult();
        return;
      }
      bool was_tokenizer_state_change_speculative = false;
      HTMLTokenizer::State state_before_speculative_state_change =
          HTMLTokenizer::kDataState;
      if (token->GetType() == HTMLToken::kStartTag &&
          token->GetName().size() > 0) {
        auto html_tag =
            lookupHTMLTag(token->GetName().data(), token->GetName().size());
        auto speculative_state = tokenizer_->SpeculativeStateForTag(html_tag);
        if (speculative_state && speculative_state != tokenizer_->GetState()) {
          state_before_speculative_state_change = tokenizer_->GetState();
          tokenizer_->SetState(*speculative_state);
          was_tokenizer_state_change_speculative = true;
        }
      }
      AppendTokenResult(token->Take(), was_tokenizer_state_change_speculative,
                        state_before_speculative_state_change);
    }
  }
}

BackgroundHTMLTokenProducer::ApplyDataResult
BackgroundHTMLTokenProducer::ApplyDataFromMainThread() {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  ApplyDataResult result;
  WTF::Vector<WTF::String> strings_to_append;
  bool end_of_file = false;
  {
    base::AutoLock auto_lock(input_lock_);
    std::swap(strings_to_append, strings_to_append_);
    end_of_file = end_of_file_;
    end_of_file_ = false;
    result.stop_and_delete_this = stop_and_delete_;
    result.input_generation = input_generation_;
  }
  if (result.stop_and_delete_this)
    return result;

  for (const auto& string : strings_to_append) {
    if (in_progress_token_data_.has_value()) {
      in_progress_token_data_->string_length_at_start_of_token +=
          string.length();
    }

    special_sequences_tracker_.UpdateIndices(string);

    input_.Append(SegmentedString(string));
  }

  if (end_of_file) {
    input_.Append(SegmentedString(String(&kEndOfFileMarker, 1)));
    if (in_progress_token_data_.has_value())
      ++in_progress_token_data_->string_length_at_start_of_token;
    input_.Close();
  }
  return result;
}

void BackgroundHTMLTokenProducer::DeleteOnTaskRunner() {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  delete this;
}

void BackgroundHTMLTokenProducer::AppendTokenResult(
    std::unique_ptr<HTMLToken> token,
    bool was_tokenizer_state_change_speculative,
    HTMLTokenizer::State state_before_speculative_state_change) {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  DCHECK(in_progress_token_data_.has_value());

  const unsigned num_chars_processed =
      in_progress_token_data_->string_length_at_start_of_token -
      input_.length();
  const unsigned num_lines_processed =
      input_.CurrentLine().ZeroBasedInt() -
      in_progress_token_data_->line_count_at_start_of_token;
  const unsigned column_position_at_end = input_.CurrentColumn().ZeroBasedInt();
  in_progress_token_data_.reset();

  AppendResultAndNotify(std::move(token), num_chars_processed,
                        num_lines_processed, column_position_at_end,
                        was_tokenizer_state_change_speculative,
                        state_before_speculative_state_change);
}

void BackgroundHTMLTokenProducer::AppendResultAndNotify(
    std::unique_ptr<HTMLToken> token,
    unsigned num_chars_processed,
    unsigned num_lines_processed,
    int column_position_at_end,
    bool was_tokenizer_state_change_speculative,
    HTMLTokenizer::State state_before_speculative_state_change) {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  {
    base::AutoLock auto_lock(results_lock_);
    if (clear_results_before_next_append_) {
      UpdateHistogramRelatedTotals();
      // Use Shrink() rather than clear() to keep the backing buffer available.
      // This is done to avoid memory churn during production.
      bg_thread_results_.Shrink(0u);
      clear_results_before_next_append_ = false;
      end_of_input_bg_thread_result_size_ = kNotFound;
    }
    bg_thread_results_.push_back(BackgroundHTMLTokenProducerParseResult());
    BackgroundHTMLTokenProducerParseResult& result = bg_thread_results_.back();
    result.token = std::move(token);
    result.num_chars_processed = num_chars_processed;
    result.num_lines_processed = num_lines_processed;
    result.column_position_at_end = column_position_at_end;
    result.was_tokenizer_state_change_speculative =
        was_tokenizer_state_change_speculative;
    result.state_before_speculative_state_change =
        state_before_speculative_state_change;

    if (result.token) {
      // When there is a valid token, the snapshot can be obtained from the
      // tokenizer.
      tokenizer_->GetSnapshot(result.tokenizer_snapshot);
    } else if (bg_thread_results_.size() > 1) {
      // This case is hit when this function is called without a token (such
      // as an unhandled sequence). Copy the tokenizer state to make the main
      // thread handling simpler (meaning it can always copy directly from
      // a result, rather than backtracking).
      result.tokenizer_snapshot =
          bg_thread_results_[bg_thread_results_.size() - 2].tokenizer_snapshot;
    } else if (!main_thread_results_.empty()) {
      // Similar to previous case, but this is the first result being added to
      // `bg_thread_results_`. In this case the last snapshot is available in
      // `main_thread_results_`.
      result.tokenizer_snapshot =
          main_thread_results_.back().tokenizer_snapshot;
    } else {
      // This is the very first result. In this case no tokens have been
      // produced, so only the state from the tokenizer is needed.
      result.tokenizer_snapshot.state = tokenizer_->GetState();
    }

    // The main thread may be blocked waiting for a token. Signal to wake it up.
    if (bg_thread_results_.size() == 1)
      results_available_.Signal();

    // When adding the max token, wait for the main thread to swap the buffers.
    while (!clear_results_before_next_append_ &&
           bg_thread_results_.size() == g_max_tokens) {
      clear_results_was_set_.Wait();
    }
  }
}

void BackgroundHTMLTokenProducer::NotifyEndOfInput(uint8_t input_generation) {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  base::AutoLock auto_lock(results_lock_);
  processed_input_generation_ = input_generation;
  results_available_.Signal();
  end_of_input_bg_thread_result_size_ =
      bg_thread_results_.empty() ? kNotFound : bg_thread_results_.size();
}

void BackgroundHTMLTokenProducer::AppendUnhandledSequenceResult() {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  AppendResultAndNotify(nullptr, 0, 0, 0, false, HTMLTokenizer::kDataState);
}

void BackgroundHTMLTokenProducer::UpdateHistogramRelatedTotals() {
  DCHECK(IsRunningOnBackgroundTaskRunner());
  // NOTE: this code path is hit right after the main thread swapped the
  // two vectors (`bg_thread_results_` and `main_thread_results_`). This
  // means the main thread is now processing `main_thread_results_`.
  const bool was_end_of_input_token =
      end_of_input_bg_thread_result_size_ == main_thread_results_.size();
  if (was_end_of_input_token) {
    total_tokens_processed_when_end_reached_ += main_thread_results_.size();
    ++num_calls_to_next_parse_results_when_end_reached_;
  } else {
    total_tokens_processed_ += main_thread_results_.size();
    ++num_calls_to_next_parse_results_;
  }
}

bool BackgroundHTMLTokenProducer::IsRunningOnMainThread() const {
  return !task_runner_->RunsTasksInCurrentSequence();
}

bool BackgroundHTMLTokenProducer::IsRunningOnBackgroundTaskRunner() const {
  return task_runner_->RunsTasksInCurrentSequence();
}

}  // namespace blink
