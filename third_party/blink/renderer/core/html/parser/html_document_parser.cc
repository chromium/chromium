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

#include "third_party/blink/renderer/core/html/parser/html_document_parser.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/stl_util.h"
#include "base/strings/strcat.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"
#include "third_party/blink/renderer/core/html/parser/html_resource_preloader.h"
#include "third_party/blink/renderer/core/html/parser/html_tree_builder.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/inspector/inspector_trace_events.h"
#include "third_party/blink/renderer/core/loader/document_loader.h"
#include "third_party/blink/renderer/core/loader/prefetched_signed_exchange_manager.h"
#include "third_party/blink/renderer/core/loader/preload_helper.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/script/html_parser_script_runner.h"
#include "third_party/blink/renderer/platform/bindings/runtime_call_stats.h"
#include "third_party/blink/renderer/platform/bindings/v8_per_isolate_data.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"

namespace blink {

// This sets the (default) maximum number of tokens which the foreground HTML
// parser should try to process in one go. Lower values generally mean faster
// first paints, larger values delay first paint, but make sure it's closer to
// the final page. This is the default value to use, if no Finch-provided
// value exists.
constexpr int kDefaultMaxTokenizationBudget = 250;

class EndIfDelayedForbiddenScope;
class ShouldCompleteScope;
class AttemptToEndForbiddenScope;

// This class encapsulates the internal state needed for synchronous foreground
// HTML parsing (e.g. if HTMLDocumentParser::PumpTokenizer yields, this class
// tracks what should be done after the pump completes.)
class HTMLDocumentParserState
    : public GarbageCollected<HTMLDocumentParserState> {
  friend EndIfDelayedForbiddenScope;
  friend ShouldCompleteScope;
  friend AttemptToEndForbiddenScope;

 public:
  // Keeps track of whether the parser needs to complete tokenization work,
  // optionally followed by EndIfDelayed.
  enum class DeferredParserState {
    // Indicates that a tokenizer pump has either completed or hasn't been
    // scheduled.
    kNotScheduled = 0,  // Enforce ordering in this enum.
    // Indicates that a tokenizer pump is scheduled and hasn't completed yet.
    kScheduled = 1,
    // Indicates that a tokenizer pump, followed by EndIfDelayed, is scheduled.
    kScheduledWithEndIfDelayed = 2
  };

  enum class MetaCSPTokenState {
    // If we've seen a meta CSP token in an upcoming HTML chunk, then we need to
    // defer any preloads until we've added the CSP token to the document and
    // applied the Content Security Policy.
    kSeen = 0,
    // Indicates that there is no meta CSP token in the upcoming chunk.
    kNotSeen = 1,
    // Indicates that we've added the CSP token to the document and we can now
    // fetch preloads.
    kProcessed = 2,
    // Indicates that it's too late to apply a Content-Security policy (because
    // we've exited the header section.)
    kUnenforceable = 3,
  };

  explicit HTMLDocumentParserState(ParserSynchronizationPolicy mode)
      : state_(DeferredParserState::kNotScheduled),
        meta_csp_state_(MetaCSPTokenState::kNotSeen),
        mode_(mode) {}

  void Trace(Visitor* v) const {}

  void SetState(DeferredParserState state) {
    DCHECK(!(state == DeferredParserState::kScheduled && ShouldComplete()));
    state_ = state;
  }
  DeferredParserState GetState() const { return state_; }

  bool IsScheduled() const { return state_ >= DeferredParserState::kScheduled; }
  const char* GetStateAsString() const {
    switch (state_) {
      case DeferredParserState::kNotScheduled:
        return "not_scheduled";
      case DeferredParserState::kScheduled:
        return "scheduled";
      case DeferredParserState::kScheduledWithEndIfDelayed:
        return "scheduled_with_end_if_delayed";
    }
  }

  bool NeedsLinkHeaderPreloadsDispatch() const {
    return needs_link_header_dispatch_;
  }
  void DispatchedLinkHeaderPreloads() { needs_link_header_dispatch_ = false; }

  bool SeenFirstByte() const { return have_seen_first_byte_; }
  void MarkSeenFirstByte() { have_seen_first_byte_ = true; }

  bool EndWasDelayed() const { return end_was_delayed_; }
  void SetEndWasDelayed(bool new_value) { end_was_delayed_ = new_value; }

  bool AddedPendingParserBlockingStylesheet() const {
    return added_pending_parser_blocking_stylesheet_;
  }
  void SetAddedPendingParserBlockingStylesheet(bool new_value) {
    added_pending_parser_blocking_stylesheet_ = new_value;
  }

  bool WaitingForStylesheets() const { return is_waiting_for_stylesheets_; }
  void SetWaitingForStylesheets(bool new_value) {
    is_waiting_for_stylesheets_ = new_value;
  }

  // Keeps track of whether Document::Finish has been called whilst parsing.
  // ShouldAttemptToEndOnEOF() means that the parser should close when there's
  // no more input.
  bool ShouldAttemptToEndOnEOF() const { return should_attempt_to_end_on_eof_; }
  void SetAttemptToEndOnEOF() {
    // Should only ever call ::Finish once.
    DCHECK(!should_attempt_to_end_on_eof_);
    // This method should only be called from ::Finish.
    should_attempt_to_end_on_eof_ = true;
  }

  bool ShouldEndIfDelayed() const { return end_if_delayed_forbidden_ == 0; }
  bool ShouldComplete() const {
    return should_complete_ || GetMode() != kAllowDeferredParsing;
  }
  bool IsSynchronous() const {
    return mode_ == ParserSynchronizationPolicy::kForceSynchronousParsing;
  }
  ParserSynchronizationPolicy GetMode() const { return mode_; }

  void MarkYield() { times_yielded_++; }
  int TimesYielded() const { return times_yielded_; }

  NestingLevelIncrementer ScopedPumpSession() {
    return NestingLevelIncrementer(pump_session_nesting_level_);
  }
  bool InPumpSession() const { return pump_session_nesting_level_; }
  bool InNestedPumpSession() const { return pump_session_nesting_level_ > 1; }

  void SetSeenCSPMetaTag(const bool seen) {
    if (meta_csp_state_ == MetaCSPTokenState::kUnenforceable)
      return;
    if (seen)
      meta_csp_state_ = MetaCSPTokenState::kSeen;
    else
      meta_csp_state_ = MetaCSPTokenState::kNotSeen;
  }

  void SetExitedHeader() {
    meta_csp_state_ = MetaCSPTokenState::kUnenforceable;
  }
  bool HaveExitedHeader() const {
    return meta_csp_state_ == MetaCSPTokenState::kUnenforceable;
  }

 private:
  void EnterEndIfDelayedForbidden() { end_if_delayed_forbidden_++; }
  void ExitEndIfDelayedForbidden() {
    DCHECK(end_if_delayed_forbidden_);
    end_if_delayed_forbidden_--;
  }

  void EnterAttemptToEndForbidden() {
    DCHECK(should_attempt_to_end_on_eof_);
    should_attempt_to_end_on_eof_ = false;
  }

  void EnterShouldComplete() { should_complete_++; }
  void ExitShouldComplete() {
    DCHECK(should_complete_);
    should_complete_--;
  }

  DeferredParserState state_;
  MetaCSPTokenState meta_csp_state_;
  ParserSynchronizationPolicy mode_;
  unsigned end_if_delayed_forbidden_ = 0;
  unsigned should_complete_ = 0;
  unsigned times_yielded_ = 0;
  unsigned pump_session_nesting_level_ = 0;

  // Set to non-zero if Document::Finish has been called and we're operating
  // asynchronously.
  bool should_attempt_to_end_on_eof_ = false;
  bool needs_link_header_dispatch_ = true;
  bool have_seen_first_byte_ = false;
  bool end_was_delayed_ = false;
  bool added_pending_parser_blocking_stylesheet_ = false;
  bool is_waiting_for_stylesheets_ = false;
};

class EndIfDelayedForbiddenScope {
  STACK_ALLOCATED();

 public:
  explicit EndIfDelayedForbiddenScope(HTMLDocumentParserState* state)
      : state_(state) {
    state_->EnterEndIfDelayedForbidden();
  }
  ~EndIfDelayedForbiddenScope() { state_->ExitEndIfDelayedForbidden(); }

 private:
  HTMLDocumentParserState* state_;
};

class AttemptToEndForbiddenScope {
  STACK_ALLOCATED();

 public:
  explicit AttemptToEndForbiddenScope(HTMLDocumentParserState* state)
      : state_(state) {
    state_->EnterAttemptToEndForbidden();
  }

 private:
  HTMLDocumentParserState* state_;
};

class ShouldCompleteScope {
  STACK_ALLOCATED();

 public:
  explicit ShouldCompleteScope(HTMLDocumentParserState* state) : state_(state) {
    state_->EnterShouldComplete();
  }
  ~ShouldCompleteScope() { state_->ExitShouldComplete(); }

 private:
  HTMLDocumentParserState* state_;
};

// This is a direct transcription of step 4 from:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#fragment-case
static HTMLTokenizer::State TokenizerStateForContextElement(
    Element* context_element,
    bool report_errors,
    const HTMLParserOptions& options) {
  if (!context_element)
    return HTMLTokenizer::kDataState;

  const QualifiedName& context_tag = context_element->TagQName();

  if (context_tag.Matches(html_names::kTitleTag) ||
      context_tag.Matches(html_names::kTextareaTag))
    return HTMLTokenizer::kRCDATAState;
  if (context_tag.Matches(html_names::kStyleTag) ||
      context_tag.Matches(html_names::kXmpTag) ||
      context_tag.Matches(html_names::kIFrameTag) ||
      context_tag.Matches(html_names::kNoembedTag) ||
      (context_tag.Matches(html_names::kNoscriptTag) &&
       options.scripting_flag) ||
      context_tag.Matches(html_names::kNoframesTag))
    return report_errors ? HTMLTokenizer::kRAWTEXTState
                         : HTMLTokenizer::kPLAINTEXTState;
  if (context_tag.Matches(html_names::kScriptTag))
    return report_errors ? HTMLTokenizer::kScriptDataState
                         : HTMLTokenizer::kPLAINTEXTState;
  if (context_tag.Matches(html_names::kPlaintextTag))
    return HTMLTokenizer::kPLAINTEXTState;
  return HTMLTokenizer::kDataState;
}

class ScopedYieldTimer {
 public:
  // This object is created at the start of a block of parsing, and will
  // report the time since the last block yielded if known.
  ScopedYieldTimer(std::unique_ptr<base::ElapsedTimer>* timer,
                   HTMLParserMetrics* metrics_reporter)
      : timer_(timer), reporting_metrics_(metrics_reporter) {
    if (!reporting_metrics_ || !(*timer_))
      return;

    metrics_reporter->AddYieldInterval((*timer_)->Elapsed());
    timer_->reset();
  }

  // The destructor creates a new timer, which will keep track of time until
  // the next block starts.
  ~ScopedYieldTimer() {
    if (reporting_metrics_)
      *timer_ = std::make_unique<base::ElapsedTimer>();
  }

 private:
  std::unique_ptr<base::ElapsedTimer>* timer_;
  bool reporting_metrics_;
};

HTMLDocumentParser::HTMLDocumentParser(HTMLDocument& document,
                                       ParserSynchronizationPolicy sync_policy,
                                       ParserPrefetchPolicy prefetch_policy)
    : HTMLDocumentParser(document,
                         kAllowScriptingContent,
                         sync_policy,
                         prefetch_policy) {
  script_runner_ =
      HTMLParserScriptRunner::Create(ReentryPermit(), &document, this);

  // Allow declarative shadow DOM for the document parser, if not explicitly
  // disabled.
  bool include_shadow_roots = document.GetDeclarativeShadowRootAllowState() !=
                              Document::DeclarativeShadowRootAllowState::kDeny;
  tree_builder_ = MakeGarbageCollected<HTMLTreeBuilder>(
      this, document, kAllowScriptingContent, options_, include_shadow_roots);
}

HTMLDocumentParser::HTMLDocumentParser(
    DocumentFragment* fragment,
    Element* context_element,
    ParserContentPolicy parser_content_policy,
    ParserPrefetchPolicy parser_prefetch_policy)
    : HTMLDocumentParser(fragment->GetDocument(),
                         parser_content_policy,
                         kForceSynchronousParsing,
                         parser_prefetch_policy) {
  // Allow declarative shadow DOM for the fragment parser only if explicitly
  // enabled.
  bool include_shadow_roots =
      fragment->GetDocument().GetDeclarativeShadowRootAllowState() ==
      Document::DeclarativeShadowRootAllowState::kAllow;

  // No script_runner_ in fragment parser.
  tree_builder_ = MakeGarbageCollected<HTMLTreeBuilder>(
      this, fragment, context_element, parser_content_policy, options_,
      include_shadow_roots);

  // For now document fragment parsing never reports errors.
  bool report_errors = false;
  tokenizer_->SetState(TokenizerStateForContextElement(
      context_element, report_errors, options_));
}

HTMLDocumentParser::HTMLDocumentParser(Document& document,
                                       ParserContentPolicy content_policy,
                                       ParserSynchronizationPolicy sync_policy,
                                       ParserPrefetchPolicy prefetch_policy)
    : ScriptableDocumentParser(document, content_policy),
      options_(&document),
      token_(std::make_unique<HTMLToken>()),
      tokenizer_(std::make_unique<HTMLTokenizer>(options_)),
      loading_task_runner_(sync_policy == kForceSynchronousParsing
                               ? nullptr
                               : document.GetTaskRunner(TaskType::kNetworking)),
      task_runner_state_(
          MakeGarbageCollected<HTMLDocumentParserState>(sync_policy)),
      scheduler_(sync_policy == kAllowDeferredParsing
                     ? Thread::Current()->Scheduler()
                     : nullptr) {
  // Report metrics for async document parsing or forced synchronous parsing.
  // The document must be main frame to meet UKM requirements, and must have a
  // high resolution clock for high quality data.
  if (sync_policy == kAllowDeferredParsing && document.GetFrame() &&
      document.GetFrame()->IsMainFrame() &&
      base::TimeTicks::IsHighResolution()) {
    metrics_reporter_ = std::make_unique<HTMLParserMetrics>(
        document.UkmSourceID(), document.UkmRecorder());
  }

  // Don't create preloader for parsing clipboard content.
  if (content_policy == kDisallowScriptingAndPluginContent)
    return;

  // Create preloader only when the document is:
  // - attached to a frame (likely the prefetched resources will be loaded
  // soon),
  // - is for no-state prefetch (made specifically for running preloader).
  if (!document.GetFrame() && !document.IsPrefetchOnly())
    return;

  if (prefetch_policy == kAllowPrefetching)
    preloader_ = MakeGarbageCollected<HTMLResourcePreloader>(document);
}

HTMLDocumentParser::~HTMLDocumentParser() = default;

void HTMLDocumentParser::Trace(Visitor* visitor) const {
  visitor->Trace(reentry_permit_);
  visitor->Trace(tree_builder_);
  visitor->Trace(script_runner_);
  visitor->Trace(preloader_);
  visitor->Trace(task_runner_state_);
  ScriptableDocumentParser::Trace(visitor);
  HTMLParserScriptRunnerHost::Trace(visitor);
}

bool HTMLDocumentParser::HasPendingWorkScheduledForTesting() const {
  return task_runner_state_->IsScheduled();
}

void HTMLDocumentParser::Detach() {
  // Deschedule any pending tokenizer pumps.
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kNotScheduled);
  DocumentParser::Detach();
  if (script_runner_)
    script_runner_->Detach();
  if (tree_builder_)
    tree_builder_->Detach();
  // FIXME: It seems wrong that we would have a preload scanner here. Yet during
  // fast/dom/HTMLScriptElement/script-load-events.html we do.
  preload_scanner_.reset();
  insertion_preload_scanner_.reset();
  // Oilpan: It is important to clear token_ to deallocate backing memory of
  // HTMLToken::data_ and let the allocator reuse the memory for
  // HTMLToken::data_ of a next HTMLDocumentParser. We need to clear
  // tokenizer_ first because tokenizer_ has a raw pointer to token_.
  // TODO(masonf): We can probably move tokenizer_ and token_ into the
  // HTMLDocumentParser itself, instead of having them as Members.
  tokenizer_.reset();
  token_.reset();
}

void HTMLDocumentParser::StopParsing() {
  DocumentParser::StopParsing();
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kNotScheduled);
}

// This kicks off "Once the user agent stops parsing" as described by:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#the-end
void HTMLDocumentParser::PrepareToStopParsing() {
  TRACE_EVENT1("blink", "HTMLDocumentParser::PrepareToStopParsing", "parser",
               (void*)this);
  DCHECK(!HasInsertionPoint());

  // If we've already been detached, e.g. in
  // WebFrameTest.SwapMainFrameWhileLoading, bail out.
  if (IsDetached())
    return;

  DCHECK(tokenizer_);

  // NOTE: This pump should only ever emit buffered character tokens.
  if (!GetDocument()->IsPrefetchOnly()) {
    ShouldCompleteScope should_complete(task_runner_state_);
    EndIfDelayedForbiddenScope should_not_end_if_delayed(task_runner_state_);
    PumpTokenizerIfPossible();
  }

  if (IsStopped())
    return;

  DocumentParser::PrepareToStopParsing();

  // We will not have a scriptRunner when parsing a DocumentFragment.
  if (script_runner_)
    GetDocument()->SetReadyState(Document::kInteractive);

  // Setting the ready state above can fire mutation event and detach us from
  // underneath. In that case, just bail out.
  if (IsDetached())
    return;

  AttemptToRunDeferredScriptsAndEnd();
}

bool HTMLDocumentParser::IsPaused() const {
  return IsWaitingForScripts() || task_runner_state_->WaitingForStylesheets();
}

bool HTMLDocumentParser::IsParsingFragment() const {
  return tree_builder_->IsParsingFragment();
}

void HTMLDocumentParser::DeferredPumpTokenizerIfPossible() {
  // This method is called asynchronously, continues building the HTML document.

  // If we're scheduled for a tokenizer pump, then document should be attached
  // and the parser should not be stopped, but sometimes a script completes
  // loading (so we schedule a pump) but the Document is stopped in the meantime
  // (e.g. fast/parser/iframe-onload-document-close-with-external-script.html).
  DCHECK(task_runner_state_->GetState() ==
             HTMLDocumentParserState::DeferredParserState::kNotScheduled ||
         !IsDetached());
  TRACE_EVENT2("blink", "HTMLDocumentParser::DeferredPumpTokenizerIfPossible",
               "parser", (void*)this, "state",
               task_runner_state_->GetStateAsString());

  // This method is called when the post task is executed, marking the end of
  // a yield. Report the yielded time.
  DCHECK(yield_timer_);
  if (metrics_reporter_) {
    metrics_reporter_->AddYieldInterval(yield_timer_->Elapsed());
  }
  yield_timer_.reset();

  bool should_call_delay_end =
      task_runner_state_->GetState() ==
      HTMLDocumentParserState::DeferredParserState::kScheduledWithEndIfDelayed;
  if (task_runner_state_->IsScheduled()) {
    task_runner_state_->SetState(
        HTMLDocumentParserState::DeferredParserState::kNotScheduled);
    if (should_call_delay_end) {
      EndIfDelayedForbiddenScope should_not_end_if_delayed(task_runner_state_);
      PumpTokenizerIfPossible();
      EndIfDelayed();
    } else {
      PumpTokenizerIfPossible();
    }
  }
}

void HTMLDocumentParser::PumpTokenizerIfPossible() {
  // This method is called synchronously, builds the HTML document up to
  // the current budget, and optionally completes.
  TRACE_EVENT1("blink", "HTMLDocumentParser::PumpTokenizerIfPossible", "parser",
               (void*)this);

  bool yielded = false;
  CheckIfBlockingStylesheetAdded();
  if (!IsStopped() &&
      (!IsPaused() || task_runner_state_->ShouldEndIfDelayed())) {
    yielded = PumpTokenizer();
  }

  if (yielded) {
    DCHECK(!task_runner_state_->ShouldComplete());
    SchedulePumpTokenizer();
  } else if (task_runner_state_->ShouldAttemptToEndOnEOF()) {
    // Fall into this branch if ::Finish has been previously called and we've
    // just finished asynchronously parsing everything.
    if (metrics_reporter_)
      metrics_reporter_->ReportMetricsAtParseEnd();
    AttemptToEnd();
  } else if (task_runner_state_->ShouldEndIfDelayed()) {
    // If we did not exceed the budget or parsed everything there was to
    // parse, check if we should complete the document.
    if (task_runner_state_->ShouldComplete() || IsStopped() || IsStopping()) {
      if (metrics_reporter_)
        metrics_reporter_->ReportMetricsAtParseEnd();
      EndIfDelayed();
    } else {
      ScheduleEndIfDelayed();
    }
  }
}

void HTMLDocumentParser::RunScriptsForPausedTreeBuilder() {
  TRACE_EVENT1("blink", "HTMLDocumentParser::RunScriptsForPausedTreeBuilder",
               "parser", (void*)this);
  DCHECK(ScriptingContentIsAllowed(GetParserContentPolicy()));

  TextPosition script_start_position = TextPosition::BelowRangePosition();
  Element* script_element =
      tree_builder_->TakeScriptToProcess(script_start_position);
  // We will not have a scriptRunner when parsing a DocumentFragment.
  if (script_runner_)
    script_runner_->ProcessScriptElement(script_element, script_start_position);
  CheckIfBlockingStylesheetAdded();
}

HTMLDocumentParser::NextTokenStatus HTMLDocumentParser::CanTakeNextToken() {
  if (IsStopped())
    return kNoTokens;

  // If we're paused waiting for a script, we try to execute scripts before
  // continuing.
  auto ret = kHaveTokens;
  if (tree_builder_->HasParserBlockingScript()) {
    RunScriptsForPausedTreeBuilder();
    ret = kHaveTokensAfterScript;
  }
  if (IsStopped() || IsPaused())
    return kNoTokens;
  return ret;
}

void HTMLDocumentParser::ForcePlaintextForTextDocument() {
  tokenizer_->SetState(HTMLTokenizer::kPLAINTEXTState);
}

bool HTMLDocumentParser::PumpTokenizer() {
  DCHECK(!GetDocument()->IsPrefetchOnly());
  DCHECK(!IsStopped());
  DCHECK(tokenizer_);
  DCHECK(token_);

  NestingLevelIncrementer session = task_runner_state_->ScopedPumpSession();

  // If we're in kForceSynchronousParsing, always run until all available input
  // is consumed.
  bool should_run_until_completion = task_runner_state_->ShouldComplete() ||
                                     task_runner_state_->IsSynchronous() ||
                                     task_runner_state_->InNestedPumpSession();

  bool is_tracing;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED("blink", &is_tracing);
  unsigned starting_bytes;
  if (is_tracing) {
    starting_bytes = input_.length();
    TRACE_EVENT_BEGIN2("blink", "HTMLDocumentParser::PumpTokenizer",
                       "should_complete", should_run_until_completion,
                       "bytes_queued", starting_bytes);
  }

  // We tell the InspectorInstrumentation about every pump, even if we end up
  // pumping nothing.  It can filter out empty pumps itself.
  // FIXME: input_.Current().length() is only accurate if we end up parsing the
  // whole buffer in this pump.  We should pass how much we parsed as part of
  // DidWriteHTML instead of WillWriteHTML.
  probe::ParseHTML probe(GetDocument(), this);

  bool should_yield = false;
  // If we've yielded more than 2 times, then set the budget to a very large
  // number, to attempt to consume all available tokens in one go. This
  // heuristic is intended to allow a quick first contentful paint, followed by
  // a larger rendering lifecycle that processes the remainder of the page.
  int budget = (task_runner_state_->TimesYielded() <= 2)
                   ? kDefaultMaxTokenizationBudget
                   : 1e7;

  base::ElapsedTimer chunk_parsing_timer_;
  unsigned tokens_parsed = 0;
  while (!should_yield) {
    const auto next_token_status = CanTakeNextToken();
    if (next_token_status == kNoTokens) {
      // No tokens left to process in this pump, so break
      break;
    } else if (next_token_status == kHaveTokensAfterScript &&
               task_runner_state_->HaveExitedHeader()) {
      // Just executed a parser-blocking script in the body. We'd probably like
      // to yield at some point soon, especially if we're in "extended budget"
      // mode. So reduce the budget back to at most the default.
      budget = std::min(budget, kDefaultMaxTokenizationBudget);
    }
    {
      RUNTIME_CALL_TIMER_SCOPE(
          V8PerIsolateData::MainThreadIsolate(),
          RuntimeCallStats::CounterId::kHTMLTokenizerNextToken);
      if (!tokenizer_->NextToken(input_.Current(), Token()))
        break;
      budget--;
      tokens_parsed++;
    }
    ConstructTreeFromHTMLToken();
    if (!should_run_until_completion && !IsPaused()) {
      DCHECK_EQ(task_runner_state_->GetMode(), kAllowDeferredParsing);

      DCHECK(base::FeatureList::IsEnabled(
                 features::kDeferBeginMainFrameDuringLoading) ||
             scheduler_->DontDeferBeginMainFrame());
      should_yield = budget <= 0 && scheduler_->DontDeferBeginMainFrame();
      should_yield |= scheduler_->ShouldYieldForHighPriorityWork();
      should_yield &= task_runner_state_->HaveExitedHeader();
    } else {
      should_yield = false;
    }
    DCHECK(IsStopped() || Token().IsUninitialized());
  }

  if (is_tracing) {
    TRACE_EVENT_END2("blink", "HTMLDocumentParser::PumpTokenizer",
                     "parsed_tokens", tokens_parsed, "parsed_bytes",
                     starting_bytes - input_.length());
  }

  if (IsStopped() || IsParsingFragment()) {
    if (metrics_reporter_ && tokens_parsed) {
      metrics_reporter_->AddChunk(chunk_parsing_timer_.Elapsed(),
                                  tokens_parsed);
    }
    return false;
  }

  // There should only be PendingText left since the tree-builder always flushes
  // the task queue before returning. In case that ever changes, crash.
  tree_builder_->Flush(kFlushAlways);
  CHECK(!IsStopped());

  if (IsPaused()) {
    DCHECK_EQ(tokenizer_->GetState(), HTMLTokenizer::kDataState);

    if (preloader_) {
      if (!preload_scanner_) {
        preload_scanner_ = CreatePreloadScanner(
            TokenPreloadScanner::ScannerType::kMainDocument);
        preload_scanner_->AppendToEnd(input_.Current());
      }
      ScanAndPreload(preload_scanner_.get());
    }
  }

  if (metrics_reporter_ && tokens_parsed) {
    metrics_reporter_->AddChunk(chunk_parsing_timer_.Elapsed(), tokens_parsed);
  }

  // should_run_until_completion implies that we should not yield
  CHECK(!should_run_until_completion || !should_yield);
  if (should_yield)
    task_runner_state_->MarkYield();
  return should_yield;
}

void HTMLDocumentParser::SchedulePumpTokenizer() {
  TRACE_EVENT0("blink", "HTMLDocumentParser::SchedulePumpTokenizer");
  DCHECK(!IsStopped());
  DCHECK(!task_runner_state_->InPumpSession());
  DCHECK(!task_runner_state_->ShouldComplete());
  if (task_runner_state_->IsScheduled()) {
    // If the parser is already scheduled, there's no need to do anything.
    return;
  }
  loading_task_runner_->PostTask(
      FROM_HERE, WTF::Bind(&HTMLDocumentParser::DeferredPumpTokenizerIfPossible,
                           WrapPersistent(this)));
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kScheduled);

  yield_timer_ = std::make_unique<base::ElapsedTimer>();
}

void HTMLDocumentParser::ScheduleEndIfDelayed() {
  TRACE_EVENT0("blink", "HTMLDocumentParser::ScheduleEndIfDelayed");
  DCHECK(!IsStopped());
  DCHECK(!task_runner_state_->InPumpSession());
  DCHECK(!task_runner_state_->ShouldComplete());

  // Schedule a pump callback if needed.
  if (!task_runner_state_->IsScheduled()) {
    loading_task_runner_->PostTask(
        FROM_HERE,
        WTF::Bind(&HTMLDocumentParser::DeferredPumpTokenizerIfPossible,
                  WrapPersistent(this)));
    yield_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  // If a pump is already scheduled, it's OK to just upgrade it to one
  // which calls EndIfDelayed afterwards.
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kScheduledWithEndIfDelayed);
}

void HTMLDocumentParser::ConstructTreeFromHTMLToken() {
  DCHECK(!GetDocument()->IsPrefetchOnly());

  AtomicHTMLToken atomic_token(Token());

  // Check whether we've exited the header.
  if (!task_runner_state_->HaveExitedHeader()) {
    if (GetDocument()->body()) {
      task_runner_state_->SetExitedHeader();
    }
  }

  // We clear the token_ in case ConstructTree() synchronously re-enters the
  // parser. We don't clear the token immediately for kCharacter tokens because
  // the AtomicHTMLToken avoids copying the characters by keeping a pointer to
  // the underlying buffer in the HTMLToken. Fortunately, kCharacter tokens
  // can't cause us to re-enter the parser.
  if (Token().GetType() != HTMLToken::kCharacter)
    Token().Clear();

  tree_builder_->ConstructTree(&atomic_token);
  CheckIfBlockingStylesheetAdded();

  // FIXME: ConstructTree may synchronously cause Document to be detached.
  if (!token_)
    return;

  if (!Token().IsUninitialized()) {
    DCHECK_EQ(Token().GetType(), HTMLToken::kCharacter);
    Token().Clear();
  }
}

bool HTMLDocumentParser::HasInsertionPoint() {
  // FIXME: The wasCreatedByScript() branch here might not be fully correct. Our
  // model of the EOF character differs slightly from the one in the spec
  // because our treatment is uniform between network-sourced and script-sourced
  // input streams whereas the spec treats them differently.
  return input_.HasInsertionPoint() ||
         (WasCreatedByScript() && !input_.HaveSeenEndOfFile());
}

void HTMLDocumentParser::insert(const String& source) {
  if (IsStopped())
    return;

  TRACE_EVENT2("blink", "HTMLDocumentParser::insert", "source_length",
               source.length(), "parser", (void*)this);

  SegmentedString excluded_line_number_source(source);
  excluded_line_number_source.SetExcludeLineNumbers();
  input_.InsertAtCurrentInsertionPoint(excluded_line_number_source);

  // Pump the the tokenizer to build the document from the given insert point.
  // Should process everything available and not defer anything.
  ShouldCompleteScope should_complete(task_runner_state_);
  EndIfDelayedForbiddenScope should_not_end_if_delayed(task_runner_state_);
  // Call EndIfDelayed manually at the end to maintain preload behaviour.
  PumpTokenizerIfPossible();

  if (IsPaused()) {
    // Check the document.write() output with a separate preload scanner as
    // the main scanner can't deal with insertions.
    if (!insertion_preload_scanner_) {
      insertion_preload_scanner_ =
          CreatePreloadScanner(TokenPreloadScanner::ScannerType::kInsertion);
    }
    insertion_preload_scanner_->AppendToEnd(source);
    if (preloader_) {
      ScanAndPreload(insertion_preload_scanner_.get());
    }
  }
  EndIfDelayed();
}

void HTMLDocumentParser::Append(const String& input_source) {
  TRACE_EVENT2("blink", "HTMLDocumentParser::append", "size",
               input_source.length(), "parser", (void*)this);

  if (IsStopped())
    return;

  const SegmentedString source(input_source);

  if (!preload_scanner_ && GetDocument()->Url().IsValid() &&
      (!task_runner_state_->IsSynchronous() ||
       GetDocument()->IsPrefetchOnly() || IsPaused())) {
    // If we're operating with a budget, we need to create a preload scanner to
    // make sure that parser-blocking Javascript requests are dispatched in
    // plenty of time, which prevents unnecessary delays.
    // When parsing without a budget (e.g. for HTML fragment parsing), it's
    // additional overhead to scan the string unless the parser's already
    // paused whilst executing a script.
    preload_scanner_ =
        CreatePreloadScanner(TokenPreloadScanner::ScannerType::kMainDocument);
  }

  if (GetDocument()->IsPrefetchOnly()) {
    preload_scanner_->AppendToEnd(source);
    if (preloader_) {
      // TODO(Richard.Townsend@arm.com): add test coverage of this branch.
      // The crash in crbug.com/1166786 indicates that text documents are being
      // speculatively prefetched.
      ScanAndPreload(preload_scanner_.get());
    }

    // Return after the preload scanner, do not actually parse the document.
    return;
  }
  if (preload_scanner_ && preloader_) {
    preload_scanner_->AppendToEnd(source);
    if (task_runner_state_->GetMode() == kAllowDeferredParsing &&
        (IsPaused() || !task_runner_state_->SeenFirstByte())) {
      // Should scan and preload if the parser's paused waiting for a resource,
      // or if we're starting a document for the first time (we want to at least
      // prefetch anything that's in the <head> section).
      ScanAndPreload(preload_scanner_.get());
    }
  }

  input_.AppendToEnd(source);
  task_runner_state_->MarkSeenFirstByte();

  // Add input_source.length() to "file size" metric.
  if (metrics_reporter_)
    metrics_reporter_->AddInput(input_source.length());

  if (task_runner_state_->InPumpSession()) {
    // We've gotten data off the network in a nested write. We don't want to
    // consume any more of the input stream now.  Do not worry.  We'll consume
    // this data in a less-nested write().
    return;
  }

  // If we are preloading, FinishAppend() will be called later in
  // CommitPreloadedData().
  if (IsPreloading())
    return;

  FinishAppend();
}

void HTMLDocumentParser::FinishAppend() {
  // Schedule a tokenizer pump to process this new data. We schedule to give
  // paint a chance to happen, and because devtools somehow depends on it
  // for js loads.
  if (task_runner_state_->GetMode() ==
          ParserSynchronizationPolicy::kAllowDeferredParsing &&
      !task_runner_state_->ShouldComplete()) {
    SchedulePumpTokenizer();
  } else {
    PumpTokenizerIfPossible();
  }
}

void HTMLDocumentParser::CommitPreloadedData() {
  if (!IsPreloading())
    return;

  SetIsPreloading(false);
  if (task_runner_state_->SeenFirstByte() && !IsStopped())
    FinishAppend();
}

void HTMLDocumentParser::end() {
  DCHECK(!IsDetached());

  // Informs the the rest of WebCore that parsing is really finished (and
  // deletes this).
  tree_builder_->Finished();

  // All preloads should be done.
  preloader_ = nullptr;

  DocumentParser::StopParsing();
}

void HTMLDocumentParser::AttemptToRunDeferredScriptsAndEnd() {
  DCHECK(IsStopping());
  DCHECK(!HasInsertionPoint());
  if (script_runner_ && !script_runner_->ExecuteScriptsWaitingForParsing())
    return;
  end();
}

bool HTMLDocumentParser::ShouldDelayEnd() const {
  return task_runner_state_->InPumpSession() || IsPaused() ||
         IsExecutingScript() || task_runner_state_->IsScheduled();
}

void HTMLDocumentParser::AttemptToEnd() {
  // finish() indicates we will not receive any more data. If we are waiting on
  // an external script to load, we can't finish parsing quite yet.
  TRACE_EVENT1("blink", "HTMLDocumentParser::AttemptToEnd", "parser",
               (void*)this);
  DCHECK(task_runner_state_->ShouldAttemptToEndOnEOF());
  AttemptToEndForbiddenScope should_not_attempt_to_end(task_runner_state_);
  // We should only be in this state once after calling Finish.
  // If there are pending scripts, future control flow should pass to
  // EndIfDelayed.
  if (ShouldDelayEnd()) {
    task_runner_state_->SetEndWasDelayed(true);
    return;
  }
  PrepareToStopParsing();
}

void HTMLDocumentParser::EndIfDelayed() {
  TRACE_EVENT1("blink", "HTMLDocumentParser::EndIfDelayed", "parser",
               (void*)this);
  ShouldCompleteScope should_complete(task_runner_state_);
  EndIfDelayedForbiddenScope should_not_end_if_delayed(task_runner_state_);
  // If we've already been detached, don't bother ending.
  if (IsDetached())
    return;

  if (!task_runner_state_->EndWasDelayed() || ShouldDelayEnd())
    return;

  task_runner_state_->SetEndWasDelayed(false);
  PrepareToStopParsing();
}

void HTMLDocumentParser::Finish() {
  ShouldCompleteScope should_complete(task_runner_state_);
  EndIfDelayedForbiddenScope should_not_end_if_delayed(task_runner_state_);
  Flush();
  if (IsDetached())
    return;

  // We're not going to get any more data off the network, so we tell the input
  // stream we've reached the end of file. finish() can be called more than
  // once, if the first time does not call end().
  if (!input_.HaveSeenEndOfFile())
    input_.MarkEndOfFile();

  // If there's any deferred work remaining, signal that we
  // want to end the document once all work's complete.
  task_runner_state_->SetAttemptToEndOnEOF();
  if (task_runner_state_->IsScheduled() && !GetDocument()->IsPrefetchOnly()) {
    return;
  }

  AttemptToEnd();
}

bool HTMLDocumentParser::IsExecutingScript() const {
  if (!script_runner_)
    return false;
  return script_runner_->IsExecutingScript();
}

OrdinalNumber HTMLDocumentParser::LineNumber() const {
  return input_.Current().CurrentLine();
}

TextPosition HTMLDocumentParser::GetTextPosition() const {
  const SegmentedString& current_string = input_.Current();
  OrdinalNumber line = current_string.CurrentLine();
  OrdinalNumber column = current_string.CurrentColumn();

  return TextPosition(line, column);
}

bool HTMLDocumentParser::IsWaitingForScripts() const {
  if (IsParsingFragment()) {
    // HTMLTreeBuilder may have a parser blocking script element, but we
    // ignore it during fragment parsing.
    DCHECK(!(tree_builder_->HasParserBlockingScript() || (script_runner_ &&
    script_runner_->HasParserBlockingScript()) || reentry_permit_->ParserPauseFlag()));
    return false;
  }

  // When the TreeBuilder encounters a </script> tag, it returns to the
  // HTMLDocumentParser where the script is transfered from the treebuilder to
  // the script runner. The script runner will hold the script until its loaded
  // and run. During any of this time, we want to count ourselves as "waiting
  // for a script" and thus run the preload scanner, as well as delay completion
  // of parsing.
  bool tree_builder_has_blocking_script =
      tree_builder_->HasParserBlockingScript();
  bool script_runner_has_blocking_script =
      script_runner_ && script_runner_->HasParserBlockingScript();
  // Since the parser is paused while a script runner has a blocking script, it
  // should never be possible to end up with both objects holding a blocking
  // script.
  DCHECK(
      !(tree_builder_has_blocking_script && script_runner_has_blocking_script));
  // If either object has a blocking script, the parser should be paused.
  return tree_builder_has_blocking_script ||
         script_runner_has_blocking_script ||
         reentry_permit_->ParserPauseFlag();
}

void HTMLDocumentParser::ResumeParsingAfterPause() {
  // This function runs after a parser-blocking script has completed.
  TRACE_EVENT1("blink", "HTMLDocumentParser::ResumeParsingAfterPause", "parser",
               (void*)this);
  DCHECK(!IsExecutingScript());
  DCHECK(!IsPaused());

  CheckIfBlockingStylesheetAdded();
  if (IsStopped() || IsPaused() || IsDetached())
    return;
  DCHECK(tokenizer_);

  insertion_preload_scanner_.reset();
  if (task_runner_state_->GetMode() == kAllowDeferredParsing &&
      !task_runner_state_->ShouldComplete() &&
      !task_runner_state_->InPumpSession()) {
    SchedulePumpTokenizer();
  } else {
    ShouldCompleteScope should_complete(task_runner_state_);
    PumpTokenizerIfPossible();
  }
}

void HTMLDocumentParser::AppendCurrentInputStreamToPreloadScannerAndScan() {
  TRACE_EVENT1(
      "blink",
      "HTMLDocumentParser::AppendCurrentInputStreamToPreloadScannerAndScan",
      "parser", (void*)this);
  DCHECK(preload_scanner_);
  DCHECK(preloader_);
  preload_scanner_->AppendToEnd(input_.Current());
  ScanAndPreload(preload_scanner_.get());
}

void HTMLDocumentParser::NotifyScriptLoaded() {
  TRACE_EVENT1("blink", "HTMLDocumentParser::NotifyScriptLoaded", "parser",
               (void*)this);
  DCHECK(script_runner_);
  DCHECK(!IsExecutingScript());

  scheduler::CooperativeSchedulingManager::AllowedStackScope
      allowed_stack_scope(scheduler::CooperativeSchedulingManager::Instance());

  if (IsStopped()) {
    return;
  }

  if (IsStopping()) {
    AttemptToRunDeferredScriptsAndEnd();
    return;
  }

  script_runner_->ExecuteScriptsWaitingForLoad();
  if (!IsPaused())
    ResumeParsingAfterPause();
}

void HTMLDocumentParser::ExecuteScriptsWaitingForResources() {
  TRACE_EVENT0("blink",
               "HTMLDocumentParser::ExecuteScriptsWaitingForResources");
  if (IsStopped())
    return;

  DCHECK(GetDocument()->IsScriptExecutionReady());

  if (task_runner_state_->WaitingForStylesheets())
    task_runner_state_->SetWaitingForStylesheets(false);

  // Document only calls this when the Document owns the DocumentParser so this
  // will not be called in the DocumentFragment case.
  DCHECK(script_runner_);
  script_runner_->ExecuteScriptsWaitingForResources();
  if (!IsPaused())
    ResumeParsingAfterPause();
}

void HTMLDocumentParser::DidAddPendingParserBlockingStylesheet() {
  // In-body CSS doesn't block painting. The parser needs to pause so that
  // the DOM doesn't include any elements that may depend on the CSS for style.
  // The stylesheet can be added and removed during the parsing of a single
  // token so don't actually set the bit to block parsing here, just track
  // the state of the added sheet in case it does persist beyond a single
  // token.
  task_runner_state_->SetAddedPendingParserBlockingStylesheet(true);
}

void HTMLDocumentParser::DidLoadAllPendingParserBlockingStylesheets() {
  // Just toggle the stylesheet flag here (mostly for synchronous sheets).
  // The document will also call into executeScriptsWaitingForResources
  // which is when the parser will re-start, otherwise it will attempt to
  // resume twice which could cause state machine issues.
  task_runner_state_->SetAddedPendingParserBlockingStylesheet(false);
}

void HTMLDocumentParser::CheckIfBlockingStylesheetAdded() {
  if (task_runner_state_->AddedPendingParserBlockingStylesheet()) {
    task_runner_state_->SetAddedPendingParserBlockingStylesheet(false);
    task_runner_state_->SetWaitingForStylesheets(true);
  }
}

void HTMLDocumentParser::ParseDocumentFragment(
    const String& source,
    DocumentFragment* fragment,
    Element* context_element,
    ParserContentPolicy parser_content_policy) {
  auto* parser = MakeGarbageCollected<HTMLDocumentParser>(
      fragment, context_element, parser_content_policy);
  parser->Append(source);
  parser->Finish();
  // Allows ~DocumentParser to assert it was detached before destruction.
  parser->Detach();
}

void HTMLDocumentParser::AppendBytes(const char* data, size_t length) {
  TRACE_EVENT2("blink", "HTMLDocumentParser::appendBytes", "size",
               (unsigned)length, "parser", (void*)this);

  DCHECK(Thread::MainThread()->IsCurrentThread());

  if (!length || IsStopped())
    return;

  DecodedDataDocumentParser::AppendBytes(data, length);
}

void HTMLDocumentParser::Flush() {
  TRACE_EVENT1("blink", "HTMLDocumentParser::Flush", "parser", (void*)this);
  // If we've got no decoder, we never received any data.
  if (IsDetached() || NeedsDecoder())
    return;
  DecodedDataDocumentParser::Flush();
}

void HTMLDocumentParser::SetDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  DCHECK(decoder);
  DecodedDataDocumentParser::SetDecoder(std::move(decoder));
}

void HTMLDocumentParser::DocumentElementAvailable() {
  TRACE_EVENT0("blink,loading", "HTMLDocumentParser::DocumentElementAvailable");
  Document* document = GetDocument();
  DCHECK(document);
  DCHECK(document->documentElement());
  Element* documentElement = GetDocument()->documentElement();
  if (documentElement->hasAttribute(u"\u26A1") ||
      documentElement->hasAttribute("amp") ||
      documentElement->hasAttribute("i-amphtml-layout")) {
    // The DocumentLoader fetches a main resource and handles the result.
    // But it may not be available if JavaScript appends HTML to the page later
    // in the page's lifetime. This can happen both from in-page JavaScript and
    // from extensions. See example callstacks linked from crbug.com/931330.
    if (document->Loader()) {
      document->Loader()->DidObserveLoadingBehavior(
          kLoadingBehaviorAmpDocumentLoaded);
    }
  }
  if (preloader_)
    FetchQueuedPreloads();
}

std::unique_ptr<HTMLPreloadScanner> HTMLDocumentParser::CreatePreloadScanner(
    TokenPreloadScanner::ScannerType scanner_type) {
  return std::make_unique<HTMLPreloadScanner>(
      options_, GetDocument()->Url(),
      std::make_unique<CachedDocumentParameters>(GetDocument()),
      MediaValuesCached::MediaValuesCachedData(*GetDocument()), scanner_type);
}

void HTMLDocumentParser::ScanAndPreload(HTMLPreloadScanner* scanner) {
  TRACE_EVENT0("blink", "HTMLDocumentParser::ScanAndPreload");
  DCHECK(preloader_);
  base::ElapsedTimer timer;
  bool seen_csp_meta_tag = false;
  absl::optional<ViewportDescription> viewport_description;
  PreloadRequestStream requests =
      scanner->Scan(GetDocument()->ValidBaseElementURL(), &viewport_description,
                    seen_csp_meta_tag);
  // Make sure that the viewport is up-to-date, so that the correct viewport
  // dimensions will be fed to the preload scanner.
  if (GetDocument()->Loader() &&
      task_runner_state_->GetMode() == kAllowDeferredParsing) {
    if (viewport_description.has_value()) {
      GetDocument()->GetStyleEngine().UpdateViewport();
    }
    if (task_runner_state_->NeedsLinkHeaderPreloadsDispatch()) {
      if (GetDocument()->Loader()->GetPrefetchedSignedExchangeManager()) {
        TRACE_EVENT0("blink",
                     "HTMLDocumentParser::DispatchSignedExchangeManager");
        // Link header preloads for prefetched signed exchanges won't be started
        // until StartPrefetchedLinkHeaderPreloads() is called. See the header
        // comment of PrefetchedSignedExchangeManager.
        GetDocument()
            ->Loader()
            ->GetPrefetchedSignedExchangeManager()
            ->StartPrefetchedLinkHeaderPreloads();
      } else {
        TRACE_EVENT0("blink", "HTMLDocumentParser::DispatchLinkHeaderPreloads");
        GetDocument()->Loader()->DispatchLinkHeaderPreloads(
            base::OptionalOrNullptr(viewport_description),
            PreloadHelper::kOnlyLoadMedia);
      }
      task_runner_state_->DispatchedLinkHeaderPreloads();
    }
  }

  task_runner_state_->SetSeenCSPMetaTag(seen_csp_meta_tag);
  for (auto& request : requests) {
    queued_preloads_.push_back(std::move(request));
  }
  FetchQueuedPreloads();
  base::UmaHistogramTimes(
      base::StrCat({"Blink.ScanAndPreloadTime", GetPreloadHistogramSuffix()}),
      timer.Elapsed());
}

void HTMLDocumentParser::FetchQueuedPreloads() {
  DCHECK(preloader_);
  TRACE_EVENT0("blink", "HTMLDocumentParser::FetchQueuedPreloads");

  if (!queued_preloads_.IsEmpty()) {
    base::ElapsedTimer timer;
    preloader_->TakeAndPreload(queued_preloads_);
    base::UmaHistogramTimes(base::StrCat({"Blink.FetchQueuedPreloadsTime",
                                          GetPreloadHistogramSuffix()}),
                            timer.Elapsed());
  }
}

std::string HTMLDocumentParser::GetPreloadHistogramSuffix() {
  bool is_main_frame = GetDocument() && GetDocument()->GetFrame() &&
                       GetDocument()->GetFrame()->IsMainFrame();
  bool have_seen_first_byte = task_runner_state_->SeenFirstByte();
  return base::StrCat({is_main_frame ? ".MainFrame" : ".Subframe",
                       have_seen_first_byte ? ".NonInitial" : ".Initial"});
}

}  // namespace blink
