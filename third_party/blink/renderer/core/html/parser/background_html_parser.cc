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

#include "third_party/blink/renderer/core/html/parser/background_html_parser.h"

#include <memory>
#include <utility>

#include "base/single_thread_task_runner.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

// On a network with high latency and high bandwidth, using a device with a fast
// CPU, we could end up speculatively tokenizing the whole document, well ahead
// of when the main-thread actually needs it. This is a waste of memory (and
// potentially time if the speculation fails). So we limit our outstanding
// tokens arbitrarily to 10,000. Our maximal memory spent speculating will be
// approximately:
// (kOutstandingTokenLimit + kPendingTokenLimit) * sizeof(CompactToken)
//
// We use a separate low and high water mark to avoid
// constantly topping off the main thread's token buffer. At time of writing,
// this is (10000 + 1000) * 28 bytes = ~308kb of memory. These numbers have not
// been tuned.
static const size_t kOutstandingTokenLimit = 10000;

// We limit our chucks to 1000 tokens, to make sure the main thread is never
// waiting on the parser thread for tokens. This was tuned in
// https://bugs.webkit.org/show_bug.cgi?id=110408.
static const size_t kPendingTokenLimit = 1000;

static_assert(kOutstandingTokenLimit > kPendingTokenLimit,
              "Outstanding token limit is applied after pending token limit.");

base::WeakPtr<BackgroundHTMLParser> BackgroundHTMLParser::Create(
    std::unique_ptr<Configuration> config,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner) {
  auto* background_parser = new BackgroundHTMLParser(
      std::move(config), std::move(loading_task_runner));
  return background_parser->weak_factory_.GetWeakPtr();
}

void BackgroundHTMLParser::Init(
    const KURL& document_url,
    std::unique_ptr<CachedDocumentParameters> cached_document_parameters,
    const MediaValuesCached::MediaValuesCachedData& media_values_cached_data,
    bool priority_hints_origin_trial_enabled) {
  TRACE_EVENT1("loading", "BackgroundHTMLParser::Init", "url",
               document_url.GetString().Utf8());
  preload_scanner_.reset(new TokenPreloadScanner(
      document_url, std::move(cached_document_parameters),
      media_values_cached_data, TokenPreloadScanner::ScannerType::kMainDocument,
      priority_hints_origin_trial_enabled));
}

BackgroundHTMLParser::Configuration::Configuration() {}

BackgroundHTMLParser::BackgroundHTMLParser(
    std::unique_ptr<Configuration> config,
    scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner)
    : token_(std::make_unique<HTMLToken>()),
      tokenizer_(std::make_unique<HTMLTokenizer>(config->options)),
      tree_builder_simulator_(config->options),
      options_(config->options),
      parser_(config->parser),
      decoder_(std::move(config->decoder)),
      loading_task_runner_(std::move(loading_task_runner)),
      pending_csp_meta_token_index_(
          HTMLDocumentParser::TokenizedChunk::kNoPendingToken),
      starting_script_(false) {}

BackgroundHTMLParser::~BackgroundHTMLParser() = default;

void BackgroundHTMLParser::AppendRawBytesFromMainThread(
    std::unique_ptr<Vector<char>> buffer) {
  TRACE_EVENT0("loading", "BackgroundHTMLParser::AppendRawBytesFromMainThread");
  DCHECK(decoder_);
  UpdateDocument(decoder_->Decode(buffer->data(), buffer->size()));
}

void BackgroundHTMLParser::AppendDecodedBytes(const String& input) {
  DCHECK(!input_.Current().IsClosed());
  input_.Append(input);
  PumpTokenizer();
}

void BackgroundHTMLParser::SetDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  DCHECK(decoder);
  decoder_ = std::move(decoder);
}

void BackgroundHTMLParser::Flush() {
  DCHECK(decoder_);
  UpdateDocument(decoder_->Flush());
}

void BackgroundHTMLParser::UpdateDocument(const String& decoded_data) {
  DocumentEncodingData encoding_data(*decoder_.get());
  if (encoding_data != last_seen_encoding_data_) {
    last_seen_encoding_data_ = encoding_data;
    if (parser_)
      parser_->DidReceiveEncodingDataFromBackgroundParser(encoding_data);
  }
  if (decoded_data.IsEmpty())
    return;

  AppendDecodedBytes(decoded_data);
}

void BackgroundHTMLParser::ResumeFrom(std::unique_ptr<Checkpoint> checkpoint) {
  parser_ = checkpoint->parser;
  token_ = std::move(checkpoint->token);
  tokenizer_ = std::move(checkpoint->tokenizer);
  tree_builder_simulator_.SetState(checkpoint->tree_builder_state);
  input_.RewindTo(checkpoint->input_checkpoint, checkpoint->unparsed_input);
  preload_scanner_->RewindTo(checkpoint->preload_scanner_checkpoint);
  starting_script_ = false;
  PumpTokenizer();
}

void BackgroundHTMLParser::StartedChunkWithCheckpoint(
    HTMLInputCheckpoint input_checkpoint) {
  // Note, we should not have to worry about the index being invalid as messages
  // from the main thread will be processed in FIFO order.
  input_.InvalidateCheckpointsBefore(input_checkpoint);
  PumpTokenizer();
}

void BackgroundHTMLParser::Finish() {
  MarkEndOfFile();
  PumpTokenizer();
}

void BackgroundHTMLParser::Stop() {
  delete this;
}

void BackgroundHTMLParser::ForcePlaintextForTextDocument() {
  // This is only used by the TextDocumentParser (a subclass of
  // HTMLDocumentParser) to force us into the PLAINTEXT state w/o using a
  // <plaintext> tag. The TextDocumentParser uses a <pre> tag for historical /
  // compatibility reasons.
  tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

void BackgroundHTMLParser::MarkEndOfFile() {
  DCHECK(!input_.Current().IsClosed());
  input_.Append(String(&kEndOfFileMarker, 1));
  input_.Close();
}

void BackgroundHTMLParser::PumpTokenizer() {
  TRACE_EVENT0("loading", "BackgroundHTMLParser::pumpTokenizer");
  HTMLTreeBuilderSimulator::SimulatedToken simulated_token =
      HTMLTreeBuilderSimulator::kOtherToken;

  // No need to start speculating until the main thread has almost caught up.
  if (input_.TotalCheckpointTokenCount() > kOutstandingTokenLimit)
    return;

  while (tokenizer_->NextToken(input_.Current(), *token_)) {
    {
      TextPosition position = TextPosition(input_.Current().CurrentLine(),
                                           input_.Current().CurrentColumn());

      CompactHTMLToken token(token_.get(), position);
      bool is_csp_meta_tag = false;
      preload_scanner_->Scan(token, input_.Current(), pending_preloads_,
                             &viewport_description_, &is_csp_meta_tag);

      simulated_token =
          tree_builder_simulator_.Simulate(token, tokenizer_.get());

      // Break chunks before a script tag is inserted and flag the chunk as
      // starting a script so the main parser can decide if it should yield
      // before processing the chunk.
      if (simulated_token == HTMLTreeBuilderSimulator::kValidScriptStart) {
        EnqueueTokenizedChunk();
        starting_script_ = true;
      }

      pending_tokens_.push_back(token);
      if (is_csp_meta_tag) {
        pending_csp_meta_token_index_ = pending_tokens_.size() - 1;
      }
    }

    token_->Clear();

    if (simulated_token == HTMLTreeBuilderSimulator::kScriptEnd ||
        simulated_token == HTMLTreeBuilderSimulator::kStyleEnd ||
        simulated_token == HTMLTreeBuilderSimulator::kLink ||
        simulated_token == HTMLTreeBuilderSimulator::kCustomElementBegin ||
        pending_tokens_.size() >= kPendingTokenLimit) {
      EnqueueTokenizedChunk();

      // If we're far ahead of the main thread, yield for a bit to avoid
      // consuming too much memory.
      if (input_.TotalCheckpointTokenCount() > kOutstandingTokenLimit)
        break;
    }
  }

  EnqueueTokenizedChunk();
}

void BackgroundHTMLParser::EnqueueTokenizedChunk() {
  if (pending_tokens_.IsEmpty())
    return;

  auto chunk = std::make_unique<HTMLDocumentParser::TokenizedChunk>();
  TRACE_EVENT_WITH_FLOW0("blink,loading",
                         "BackgroundHTMLParser::sendTokensToMainThread",
                         chunk.get(), TRACE_EVENT_FLAG_FLOW_OUT);

  chunk->preloads.swap(pending_preloads_);
  if (viewport_description_.has_value())
    chunk->viewport = viewport_description_;
  chunk->tokenizer_state = tokenizer_->GetState();
  chunk->tree_builder_state = tree_builder_simulator_.GetState();
  chunk->input_checkpoint = input_.CreateCheckpoint(pending_tokens_.size());
  chunk->preload_scanner_checkpoint = preload_scanner_->CreateCheckpoint();
  chunk->tokens.swap(pending_tokens_);
  chunk->starting_script = starting_script_;
  chunk->pending_csp_meta_token_index = pending_csp_meta_token_index_;
  starting_script_ = false;
  pending_csp_meta_token_index_ =
      HTMLDocumentParser::TokenizedChunk::kNoPendingToken;

  if (parser_)
    parser_->EnqueueTokenizedChunk(std::move(chunk));
}

}  // namespace blink
