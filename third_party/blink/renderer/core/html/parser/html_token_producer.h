// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKEN_PRODUCER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKEN_PRODUCER_H_

#include <memory>

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/html/parser/background_html_token_producer.h"
#include "third_party/blink/renderer/core/html/parser/html_input_stream.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace base {
class SequencedTaskRunner;
}

namespace WTF {
class String;
}

namespace blink {
class HTMLToken;
class BackgroundHTMLTokenProducer;
struct BackgroundHTMLTokenProducerParseResult;

// HTMLTokenProducer is responsible for producing HTMLTokens. It may do the
// production on a background thread by using BackgroundHTMLTokenProducer.
// Producing tokens on the background thread has a number of limitations, if a
// scenario is encountered that can not be handled, production switches to
// running on the foreground thread. See BackgroundHTMLTokenProducer for the
// specifics.
//
// TODO(https://crbug.com/1345267): it probably makes sense that this class
// owns the inputstream. At a minimum HTMLDocumentParser shouldn't expose
// InputStream (through HTMLParserScriptRunnerHost).
class CORE_EXPORT HTMLTokenProducer {
  USING_FAST_MALLOC(HTMLTokenProducer);

 public:
  // `can_use_background_token_producer` indicates whether tokens can be
  // produced on a background thread. Whether a background thread is used is
  // gated by a feature.
  HTMLTokenProducer(HTMLInputStream& input_stream,
                    const HTMLParserOptions& parser_options,
                    bool can_use_background_token_producer,
                    HTMLTokenizer::State initial_state);
  HTMLTokenProducer(const HTMLTokenProducer&) = delete;
  HTMLTokenProducer& operator=(const HTMLTokenProducer&) = delete;
  ~HTMLTokenProducer();

  // Forces plaintext. It is assumed this is called early on (before any
  // tokens have been requested) and that `can_use_background_token_producer`
  // is false.
  void ForcePlaintext();

  // Called if a scenario is encountered where production can not be run in
  // the background. When called production switches to running in the current
  // thread.
  void AbortBackgroundParsingForDocumentWrite() {
    AbortBackgroundParsingImpl(
        BackgroundHTMLTokenProducerShutdownReason::kDocumentWrite);
  }

  // Returns the next token. The return value is owned by this class and only
  // valid until ParseNextToken() is called. This returns null if no more tokens
  // are available.
  HTMLToken* ParseNextToken();

  // Clears the token.
  ALWAYS_INLINE void ClearToken() {
    // Background parsing creates a unique token every time, so need to clear
    // it.
    if (!IsUsingBackgroundProducer())
      tokenizer_->ClearToken();
  }

  // Appends `string` to the end of text to parse.
  void AppendToEnd(const String& string);

  // Marks the end of the file.
  void MarkEndOfFile();

  // Returns true if parsing is happening on a background thread.
  ALWAYS_INLINE bool IsUsingBackgroundProducer() const {
    return background_producer_ != nullptr;
  }

#if DCHECK_IS_ON()
  // This function is really only for assertions.
  HTMLTokenizer::State GetCurrentTokenizerState() const {
    if (IsUsingBackgroundProducer()) {
      DCHECK(results_ && !results_->empty());
      return CurrentBackgroundProducerResult().tokenizer_snapshot.state;
    }
    return tokenizer_->GetState();
  }
#endif

  // These functions are exposed for the tree builder to set tokenizer state.
  // This code is on critical path, so inlined.
  ALWAYS_INLINE void SetTokenizerState(HTMLTokenizer::State state) {
    if (!IsUsingBackgroundProducer()) {
      tokenizer_->SetState(state);
      return;
    }
    was_tokenizer_state_explicitly_set_ = true;
    if (state != CurrentBackgroundProducerResult().tokenizer_snapshot.state) {
      AbortBackgroundParsingImpl(
          BackgroundHTMLTokenProducerShutdownReason::kStateMismatch);
      DCHECK(tokenizer_);
      tokenizer_->SetState(state);
    }
  }
  void SetForceNullCharacterReplacement(bool value) {
    if (!IsUsingBackgroundProducer()) {
      tokenizer_->SetForceNullCharacterReplacement(value);
      return;
    }
    force_null_character_replacement_ = value;
  }
  void SetShouldAllowCDATA(bool value) {
    if (!IsUsingBackgroundProducer()) {
      tokenizer_->SetShouldAllowCDATA(value);
      return;
    }
    should_allow_cdata_ = value;
  }

 private:
  // Moves production to the current thread. Does nothing if production already
  // occurring on the current thread.
  void AbortBackgroundParsingImpl(
      BackgroundHTMLTokenProducerShutdownReason reason);

  // Advances the text to parse.
  void AdvanceInput(const BackgroundHTMLTokenProducerParseResult& parse_result);

  // Returns the current token from the background producer.
  ALWAYS_INLINE const BackgroundHTMLTokenProducerParseResult&
  CurrentBackgroundProducerResult() const {
    DCHECK(results_ && current_result_index_ < results_->size());
    return (*results_)[current_result_index_];
  }
  ALWAYS_INLINE BackgroundHTMLTokenProducerParseResult&
  CurrentBackgroundProducerResult() {
    return const_cast<BackgroundHTMLTokenProducerParseResult&>(
        const_cast<const HTMLTokenProducer*>(this)
            ->CurrentBackgroundProducerResult());
  }

  using Results = WTF::Vector<BackgroundHTMLTokenProducerParseResult>;

  // Common state:
  HTMLInputStream& input_stream_;

  const HTMLParserOptions parser_options_;

  // The initial state for the tokenizer.
  const HTMLTokenizer::State initial_state_;

  // State used when tokenizer runs on main thread:

  // Used if production happening on the current thread.
  std::unique_ptr<HTMLTokenizer> tokenizer_;

  // The remaining state is only used when the tokenizer runs off the main
  // thread.

  // Set to true if SetTokenizerState() was called.
  bool was_tokenizer_state_explicitly_set_ = false;

  // TaskRunner the background producer runs on.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // This deletes itself once ShutdownAndScheduleDeletion() is called.
  BackgroundHTMLTokenProducer* background_producer_ = nullptr;

  // Results from the background producer. This is owned by the background
  // producer and valid until the next call to
  // BackgroundHTMLTokenProducer::NextParseResults().
  Results* results_ = nullptr;
  wtf_size_t current_result_index_ = 0u;

  // State set from the tree builder.
  bool force_null_character_replacement_ = false;
  bool should_allow_cdata_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_TOKEN_PRODUCER_H_
