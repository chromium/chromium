/*
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_DOCUMENT_PARSER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_DOCUMENT_PARSER_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/parser_content_policy.h"
#include "third_party/blink/renderer/core/dom/scriptable_document_parser.h"
#include "third_party/blink/renderer/core/html/parser/background_html_input_stream.h"
#include "third_party/blink/renderer/core/html/parser/html_input_stream.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_options.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_reentry_permit.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_tokenizer.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder_simulator.h"
#include "third_party/blink/renderer/core/html/parser/parser_synchronization_policy.h"
#include "third_party/blink/renderer/core/html/parser/preload_request.h"
#include "third_party/blink/renderer/core/html/parser/text_resource_decoder.h"
#include "third_party/blink/renderer/core/page/viewport_description.h"
#include "third_party/blink/renderer/core/script/html_parser_script_runner_host.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/deque.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"

namespace blink {

class BackgroundHTMLParser;
class CompactHTMLToken;
class Document;
class DocumentEncodingData;
class DocumentFragment;
class Element;
class HTMLDocument;
class HTMLParserMetrics;
class HTMLParserScheduler;
class HTMLParserScriptRunner;
class HTMLPreloadScanner;
class HTMLResourcePreloader;
class HTMLTreeBuilder;
class HTMLDocumentParserState;

// TODO(https://crbug.com/1049898): These are only exposed to make it possible
// to delete an expired histogram. The test should be rewritten to test at a
// different level, so it won't have to make assertions about internal state.
void CORE_EXPORT ResetDiscardedTokenCountForTesting();
size_t CORE_EXPORT GetDiscardedTokenCountForTesting();

class CORE_EXPORT HTMLDocumentParser : public ScriptableDocumentParser,
                                       private HTMLParserScriptRunnerHost {
  USING_PRE_FINALIZER(HTMLDocumentParser, Dispose);

 public:
  HTMLDocumentParser(HTMLDocument&, ParserSynchronizationPolicy);
  HTMLDocumentParser(DocumentFragment*,
                     Element* context_element,
                     ParserContentPolicy);
  ~HTMLDocumentParser() override;
  void Trace(Visitor*) const override;

  // TODO(alexclarke): Remove when background parser goes away.
  void Dispose();

  // Exposed for HTMLParserScheduler
  void ResumeParsingAfterYield();

  static void ParseDocumentFragment(
      const String&,
      DocumentFragment*,
      Element* context_element,
      ParserContentPolicy = kAllowScriptingContent);

  // Exposed for testing.
  HTMLParserScriptRunnerHost* AsHTMLParserScriptRunnerHostForTesting() {
    return this;
  }
  // Returns true if any tokenizer pumps / end if delayed / asynchronous work is
  // scheduled. Exposed so that tests can check that the parser's exited in a
  // good state.
  bool HasPendingWorkScheduledForTesting() const;

  HTMLTokenizer* Tokenizer() const { return tokenizer_.get(); }

  TextPosition GetTextPosition() const final;
  bool IsParsingAtLineNumber() const final;
  OrdinalNumber LineNumber() const final;

  HTMLParserReentryPermit* ReentryPermit() { return reentry_permit_.get(); }

  struct TokenizedChunk {
    USING_FAST_MALLOC(TokenizedChunk);

   public:
    CompactHTMLTokenStream tokens;
    PreloadRequestStream preloads;
    base::Optional<ViewportDescription> viewport;
    HTMLTokenizer::State tokenizer_state;
    HTMLTreeBuilderSimulator::State tree_builder_state;
    HTMLInputCheckpoint input_checkpoint;
    TokenPreloadScannerCheckpoint preload_scanner_checkpoint;
    bool starting_script;
    // Index into |tokens| of the last <meta> csp tag in |tokens|. Preloads will
    // be deferred until this token is parsed. Will be noPendingToken if there
    // are no csp tokens.
    int pending_csp_meta_token_index;

    static constexpr int kNoPendingToken = -1;
  };
  void EnqueueTokenizedChunk(std::unique_ptr<TokenizedChunk>);
  void DidReceiveEncodingDataFromBackgroundParser(const DocumentEncodingData&);

  void AppendBytes(const char* bytes, size_t length) override;
  void Flush() final;
  void SetDecoder(std::unique_ptr<TextResourceDecoder>) final;

  void SetMaxTokenizationBudgetForTesting(int budget) {
    max_tokenization_budget_ = budget;
  }

 protected:
  void insert(const String&) final;
  void Append(const String&) override;
  void Finish() final;

  HTMLTreeBuilder* TreeBuilder() const { return tree_builder_.Get(); }

  void ForcePlaintextForTextDocument();

 private:
  HTMLDocumentParser(Document&,
                     ParserContentPolicy,
                     ParserSynchronizationPolicy);

  // DocumentParser
  void Detach() final;
  bool HasInsertionPoint() final;
  void PrepareToStopParsing() final;
  void StopParsing() final;
  bool IsPaused() const {
    return IsWaitingForScripts() || is_waiting_for_stylesheets_;
  }
  bool IsWaitingForScripts() const final;
  bool IsExecutingScript() const final;
  void ExecuteScriptsWaitingForResources() final;
  void DidAddPendingParserBlockingStylesheet() final;
  void DidLoadAllPendingParserBlockingStylesheets() final;
  void CheckIfBlockingStylesheetAdded();
  void DocumentElementAvailable() override;

  // HTMLParserScriptRunnerHost
  void NotifyScriptLoaded() final;
  HTMLInputStream& InputStream() final { return input_; }
  bool HasPreloadScanner() const final {
    return preload_scanner_.get() && !CanParseAsynchronously();
  }
  void AppendCurrentInputStreamToPreloadScannerAndScan() final;

  void StartBackgroundParser();
  void StopBackgroundParser();
  void ValidateSpeculations(std::unique_ptr<TokenizedChunk> last_chunk);
  void DiscardSpeculationsAndResumeFrom(
      std::unique_ptr<TokenizedChunk> last_chunk,
      std::unique_ptr<HTMLToken>,
      std::unique_ptr<HTMLTokenizer>);
  size_t ProcessTokenizedChunkFromBackgroundParser(
      std::unique_ptr<TokenizedChunk>,
      bool*);
  void PumpPendingSpeculations();

  bool CanTakeNextToken();
  bool PumpTokenizer();
  void PumpTokenizerIfPossible();
  void DeferredPumpTokenizerIfPossible();
  void SchedulePumpTokenizer();
  void ScheduleEndIfDelayed();
  void ConstructTreeFromHTMLToken();
  void ConstructTreeFromCompactHTMLToken(const CompactHTMLToken&);

  void RunScriptsForPausedTreeBuilder();
  void ResumeParsingAfterPause();

  // AttemptToEnd stops document parsing if nothing's currently delaying the end
  // of parsing.
  void AttemptToEnd();
  // EndIfDelayed stops document parsing if AttemptToEnd was previously delayed,
  // or if there are no scripts/resources/nested pumps delaying the end of
  // parsing.
  void EndIfDelayed();
  void AttemptToRunDeferredScriptsAndEnd();
  void end();

  bool CanParseAsynchronously() const { return can_parse_asynchronously_; }

  bool IsParsingFragment() const;
  bool IsScheduledForUnpause() const;
  bool InPumpSession() const { return pump_session_nesting_level_ > 0; }
  // ShouldDelayEnd assesses whether any resources, scripts or nested pumps are
  // delaying the end of parsing.
  bool ShouldDelayEnd() const;

  std::unique_ptr<HTMLPreloadScanner> CreatePreloadScanner(
      TokenPreloadScanner::ScannerType);

  // Let the given HTMLPreloadScanner scan the input it has, and then preloads
  // resources using the resulting PreloadRequests and |preloader_|.
  void ScanAndPreload(HTMLPreloadScanner*);
  void FetchQueuedPreloads();

  HTMLToken& Token() { return *token_; }

  HTMLParserOptions options_;
  HTMLInputStream input_;
  scoped_refptr<HTMLParserReentryPermit> reentry_permit_;

  std::unique_ptr<HTMLToken> token_;
  std::unique_ptr<HTMLTokenizer> tokenizer_;
  Member<HTMLParserScriptRunner> script_runner_;
  Member<HTMLTreeBuilder> tree_builder_;

  std::unique_ptr<HTMLPreloadScanner> preload_scanner_;
  // A scanner used only for input provided to the insert() method.
  std::unique_ptr<HTMLPreloadScanner> insertion_preload_scanner_;

  scoped_refptr<base::SingleThreadTaskRunner> loading_task_runner_;
  Member<HTMLParserScheduler> parser_scheduler_;
  TextPosition text_position_;

  // FIXME: last_chunk_before_pause_, tokenizer_, token_, and input_ should be
  // combined into a single state object so they can be set and cleared together
  // and passed between threads together.
  std::unique_ptr<TokenizedChunk> last_chunk_before_pause_;
  Deque<std::unique_ptr<TokenizedChunk>> speculations_;
  // Using WeakPtr for GarbageCollected is discouraged. But in this case this is
  // ok because HTMLDocumentParser guarantees to revoke all WeakPtrs in the pre
  // finalizer.
  base::WeakPtr<BackgroundHTMLParser> background_parser_;
  Member<HTMLResourcePreloader> preloader_;
  Member<HTMLDocumentParserState> task_runner_state_;
  PreloadRequestStream queued_preloads_;

  // Metrics gathering and reporting
  std::unique_ptr<HTMLParserMetrics> metrics_reporter_;
  // A timer for how long we are inactive after yielding
  std::unique_ptr<base::ElapsedTimer> yield_timer_;

  // If this is non-null, then there is a meta CSP token somewhere in the
  // speculation buffer. Preloads will be deferred until a token matching this
  // pointer is parsed and the CSP policy is applied. Note that this pointer
  // tracks the *last* meta token in the speculation buffer, so it overestimates
  // how long to defer preloads. This is for simplicity, as the alternative
  // would require keeping track of token positions of preload requests.
  CompactHTMLToken* pending_csp_meta_token_;

  int max_tokenization_budget_;
  bool can_parse_asynchronously_;
  bool end_was_delayed_;
  bool have_background_parser_;
  unsigned pump_session_nesting_level_;
  unsigned pump_speculations_session_nesting_level_;
  bool is_parsing_at_line_number_;
  bool tried_loading_link_headers_;
  bool added_pending_parser_blocking_stylesheet_;
  bool is_waiting_for_stylesheets_;
  ThreadScheduler* scheduler_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_PARSER_HTML_DOCUMENT_PARSER_H_
