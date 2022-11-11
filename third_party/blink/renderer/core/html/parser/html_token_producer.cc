// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/parser/html_token_producer.h"

#include "base/feature_list.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/core/html/parser/background_html_token_producer.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/scheduler/public/worker_pool.h"

namespace blink {
namespace {

// Max number of background producers allowed at a single time. As each
// background producer effectively takes a thread, a limit is imposed. This is
// especially important for some scenarios that may trigger a bunch of producers
// to be created. For example,
// external/wpt/html/browsers/the-window-object/window-open-windowfeatures-values.html
// . This number was chosen based on there generally not being that many main
// frame navigations happening concurrently in a particular renderer.
constexpr uint8_t kMaxNumBgProducers = 8;

// Current number of background producers.
uint8_t g_num_bg_producers = 0;

}  // namespace

HTMLTokenProducer::HTMLTokenProducer(HTMLInputStream& input_stream,
                                     const HTMLParserOptions& parser_options,
                                     bool can_use_background_token_producer,
                                     HTMLTokenizer::State initial_state)
    : input_stream_(input_stream),
      parser_options_(parser_options),
      initial_state_(initial_state),
      tokenizer_(std::make_unique<HTMLTokenizer>(parser_options_)) {
  tokenizer_->SetState(initial_state);
  if (can_use_background_token_producer &&
      base::FeatureList::IsEnabled(features::kThreadedHtmlTokenizer) &&
      g_num_bg_producers < kMaxNumBgProducers) {
    ++g_num_bg_producers;
    // The main thread will block on results from the background thread. Create
    // a dedicated thread to ensure the work is scheduled. Using a normal
    // worker pool may mean the background task is never scheduled or scheduled
    // after a delay (because the worker pool has a limit to how many active
    // threads there may be).
    task_runner_ = base::ThreadPool::CreateSingleThreadTaskRunner(
        {base::TaskPriority::USER_BLOCKING, base::WithBaseSyncPrimitives()},
        base::SingleThreadTaskRunnerThreadMode::DEDICATED);
    // BackgroundHTMLTokenProducer deletes itself when
    // ShutdownAndScheduleDeletion() is called.
    background_producer_ = new BackgroundHTMLTokenProducer(
        {}, std::move(tokenizer_), task_runner_);
  }
}

HTMLTokenProducer::~HTMLTokenProducer() {
  tokenizer_.reset();

  if (IsUsingBackgroundProducer()) {
    DCHECK_GT(g_num_bg_producers, 0);
    --g_num_bg_producers;
    background_producer_->ShutdownAndScheduleDeletion(
        BackgroundHTMLTokenProducerShutdownReason::kDone);
    background_producer_ = nullptr;
  }
}

void HTMLTokenProducer::ForcePlaintext() {
  // It is assumed that if plaintext is going to be used the constructor was
  // supplied false for `can_use_background_token_producer`.
  DCHECK(!IsUsingBackgroundProducer());
  tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

void HTMLTokenProducer::AbortBackgroundParsingImpl(
    BackgroundHTMLTokenProducerShutdownReason reason) {
  if (!IsUsingBackgroundProducer())
    return;

  tokenizer_ = std::make_unique<HTMLTokenizer>(parser_options_);
  if (results_ && !results_->empty()) {
    // If abort is called after ParseNextToken() when no more data is available,
    // `current_result_index_` will be > than `current_result_index_`.
    const wtf_size_t index =
        std::min(current_result_index_, results_->size() - 1);
    BackgroundHTMLTokenProducerParseResult& parser_result = (*results_)[index];
    tokenizer_->RestoreSnapshot(parser_result.tokenizer_snapshot);
    if (parser_result.was_tokenizer_state_change_speculative)
      tokenizer_->SetState(parser_result.state_before_speculative_state_change);
  } else {
    // Abort was called before the first token is available.
    tokenizer_->SetState(initial_state_);
  }
  tokenizer_->SetForceNullCharacterReplacement(
      force_null_character_replacement_);
  tokenizer_->SetShouldAllowCDATA(should_allow_cdata_);
  background_producer_->ShutdownAndScheduleDeletion(reason);
  DCHECK_GT(g_num_bg_producers, 0);
  --g_num_bg_producers;
  background_producer_ = nullptr;
  results_ = nullptr;
}

HTMLToken* HTMLTokenProducer::ParseNextToken() {
  if (IsUsingBackgroundProducer()) {
    if (results_ && current_result_index_ < results_->size() &&
        (!was_tokenizer_state_explicitly_set_ &&
         CurrentBackgroundProducerResult()
             .was_tokenizer_state_change_speculative)) {
      // This is hit if the background producer changed the tokenizer state
      // but the tree builder did not change the state. When this happens
      // future background token production is using the wrong state, and could
      // be wrong. For this reason background production must be stopped.
      AbortBackgroundParsingImpl(
          BackgroundHTMLTokenProducerShutdownReason::kStateMismatch);
    } else {
      was_tokenizer_state_explicitly_set_ = false;
      ++current_result_index_;
      if (!results_ || current_result_index_ >= results_->size()) {
        auto* next_results = background_producer_->NextParseResults();
        if (!next_results) {
          // No more tokens to parse. BackgroundHTMLTokenProducer keeps results
          // valid until NextParseResults() is called and more input is
          // available. This way this code always has tokenizer state to restore
          // (except for initial creation with not enough data for a single
          // token, but that case doesn't require state to restore).
          return nullptr;
        }
        // The background producer should never return an empty results vector.
        results_ = next_results;
        DCHECK(!results_->empty());
        current_result_index_ = 0;
      }
      BackgroundHTMLTokenProducerParseResult& current_result =
          CurrentBackgroundProducerResult();
      if (current_result.token) {
        AdvanceInput(current_result);
        return current_result.token.get();
      }
      // If the background producer did not provide a token, then a sequence
      // was encountered that may be treated differently depending upon the
      // value of `should_allow_cdata_` or `force_null_character_replacement_`.
      // As the background producer never changes the values of
      // `should_allow_cdata_` or `force_null_character_replacement_` when a
      // special sequence is encountered, background production must be stopped.
      AbortBackgroundParsingImpl(
          BackgroundHTMLTokenProducerShutdownReason::kSpecialSequence);
    }
    // We only get here if background production was aborted and we need to
    // fall through to using `tokenizer_`.
    DCHECK(!IsUsingBackgroundProducer());
  }
  return tokenizer_->NextToken(input_stream_.Current());
}

void HTMLTokenProducer::AppendToEnd(const String& string) {
  if (IsUsingBackgroundProducer())
    background_producer_->AppendToEnd(string);
}

void HTMLTokenProducer::MarkEndOfFile() {
  if (IsUsingBackgroundProducer())
    background_producer_->MarkEndOfFile();
}

void HTMLTokenProducer::AdvanceInput(
    const BackgroundHTMLTokenProducerParseResult& parse_result) {
  input_stream_.Current().Advance(parse_result.num_chars_processed,
                                  parse_result.num_lines_processed,
                                  parse_result.column_position_at_end);
}

}  // namespace blink
