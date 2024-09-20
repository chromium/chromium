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

#include "base/feature_list.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/strcat.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/types/optional_util.h"
#include "components/miracle_parameter/common/public/miracle_parameter.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/loader/loading_behavior_flag.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/core/css/media_values_cached.h"
#include "third_party/blink/renderer/core/css/style_engine.h"
#include "third_party/blink/renderer/core/dom/document_fragment.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/execution_context/agent.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/html_document.h"
#include "third_party/blink/renderer/core/html/html_meta_element.h"
#include "third_party/blink/renderer/core/html/html_template_element.h"
#include "third_party/blink/renderer/core/html/nesting_level_incrementer.h"
#include "third_party/blink/renderer/core/html/parser/atomic_html_token.h"
#include "third_party/blink/renderer/core/html/parser/background_html_scanner.h"
#include "third_party/blink/renderer/core/html/parser/html_element_stack.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_metrics.h"
#include "third_party/blink/renderer/core/html/parser/html_preload_scanner.h"
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
#include "third_party/blink/renderer/platform/heap/cross_thread_handle.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/scheduler/public/cooperative_scheduling_manager.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread_scheduler.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_copier_base.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/blink/renderer/platform/wtf/wtf.h"

namespace blink {

// This sets the (default) maximum number of tokens which the foreground HTML
// parser should try to process in one go. Lower values generally mean faster
// first paints, larger values delay first paint, but make sure it's closer to
// the final page. This is the default value to use, if no Finch-provided
// value exists.
constexpr int kDefaultMaxTokenizationBudget = 250;
constexpr int kInfiniteTokenizationBudget = 1e7;
constexpr int kNumYieldsWithDefaultBudget = 2;

class EndIfDelayedForbiddenScope;
class ShouldCompleteScope;
class AttemptToEndForbiddenScope;

enum class FeatureResetMode {
  kUseCached,
  kResetForTesting,
};

const char kHistogramScanAndPreloadTime[] = "Blink.ScanAndPreloadTime2";

bool ThreadedPreloadScannerEnabled(
    FeatureResetMode reset_mode = FeatureResetMode::kUseCached) {
  // Cache the feature value since checking for each parser regresses some micro
  // benchmarks.
  static bool kEnabled =
      base::FeatureList::IsEnabled(features::kThreadedPreloadScanner);
  if (reset_mode == FeatureResetMode::kResetForTesting)
    kEnabled = base::FeatureList::IsEnabled(features::kThreadedPreloadScanner);
  return kEnabled;
}

bool TimedParserBudgetEnabled() {
  // Cache the feature value since checking for each parser regresses some micro
  // benchmarks.
  static const bool kEnabled =
      base::FeatureList::IsEnabled(features::kTimedHTMLParserBudget);
  return kEnabled;
}

bool CheckParserBudgetLessOften() {
  // Cache the feature value since checking for each parser regresses some micro
  // benchmarks.
  static const bool kEnabled =
      base::FeatureList::IsEnabled(features::kCheckHTMLParserBudgetLessOften);
  return kEnabled;
}

bool PrecompileInlineScriptsEnabled(
    FeatureResetMode reset_mode = FeatureResetMode::kUseCached) {
  // Cache the feature value since checking for each parser regresses some micro
  // benchmarks.
  static bool kEnabled =
      base::FeatureList::IsEnabled(features::kPrecompileInlineScripts);
  if (reset_mode == FeatureResetMode::kResetForTesting)
    kEnabled = base::FeatureList::IsEnabled(features::kPrecompileInlineScripts);
  return kEnabled;
}

NonMainThread* GetPreloadScannerThread() {
  DCHECK(ThreadedPreloadScannerEnabled());

  // The preload scanner relies on parsing CSS, which requires creating garbage
  // collected objects. This means the thread the scanning runs on must be GC
  // enabled.
  DEFINE_STATIC_LOCAL(
      std::unique_ptr<NonMainThread>, preload_scanner_thread,
      (NonMainThread::CreateThread(
          ThreadCreationParams(ThreadType::kPreloadScannerThread)
              .SetSupportsGC(true))));
  return preload_scanner_thread.get();
}

PreloadProcessingMode GetPreloadProcessingMode() {
  if (!ThreadedPreloadScannerEnabled())
    return PreloadProcessingMode::kNone;

  static const base::FeatureParam<PreloadProcessingMode>::Option
      kPreloadProcessingModeOptions[] = {
          {PreloadProcessingMode::kNone, "none"},
          {PreloadProcessingMode::kImmediate, "immediate"},
          {PreloadProcessingMode::kYield, "yield"},
      };

  static const base::FeatureParam<PreloadProcessingMode>
      kPreloadProcessingModeParam{
          &features::kThreadedPreloadScanner, "preload-processing-mode",
          PreloadProcessingMode::kImmediate, &kPreloadProcessingModeOptions};

  // Cache the value to avoid parsing the param string more than once.
  static const PreloadProcessingMode kPreloadProcessingModeValue =
      kPreloadProcessingModeParam.Get();
  return kPreloadProcessingModeValue;
}

bool BackgroundScanMainFrameOnly() {
  static const base::FeatureParam<bool> kScanMainFrameOnlyParam{
      &features::kThreadedPreloadScanner, "scan-main-frame-only", true};
  // Cache the value to avoid parsing the param string more than once.
  static const bool kScanMainFrameOnlyValue = kScanMainFrameOnlyParam.Get();
  return kScanMainFrameOnlyValue;
}

bool IsPreloadScanningEnabled(Document* document) {
  if (BackgroundScanMainFrameOnly() && !document->IsInOutermostMainFrame())
    return false;
  return document->GetSettings() &&
         document->GetSettings()->GetDoHtmlPreloadScanning();
}

MIRACLE_PARAMETER_FOR_TIME_DELTA(GetDefaultParserBudget,
                                 features::kTimedHTMLParserBudget,
                                 "default-parser-budget",
                                 base::Milliseconds(10))

// These constants were chosen using experiment data from the field to
// optimize Core Web Vitals metrics: https://web.dev/vitals/#core-web-vitals
// Experiments were run on both Android and desktop to determine the values
// that gave the best aggregate CWV pass rate.
constexpr int kNumYieldsWithDefaultBudgetDefaultValue =
#if BUILDFLAG(IS_ANDROID)
    2
#else
    6
#endif
    ;

MIRACLE_PARAMETER_FOR_INT(GetNumYieldsWithDefaultBudget,
                          features::kTimedHTMLParserBudget,
                          "num-yields-with-default-budget",
                          kNumYieldsWithDefaultBudgetDefaultValue)

// These constants were chosen using experiment data from the field to
// optimize Core Web Vitals metrics: https://web.dev/vitals/#core-web-vitals
// Experiments were run on both Android and desktop to determine the values
// that gave the best aggregate CWV pass rate.
constexpr base::TimeDelta kLongParserBudgetDefaultValue =
#if BUILDFLAG(IS_ANDROID)
    base::Milliseconds(50)
#else
    base::Milliseconds(500)
#endif
    ;

MIRACLE_PARAMETER_FOR_TIME_DELTA(GetLongParserBudget,
                                 features::kTimedHTMLParserBudget,
                                 "long-parser-budget",
                                 kLongParserBudgetDefaultValue)

base::TimeDelta GetDefaultTimedBudget() {
  // Cache the value to avoid parsing the param string more than once.
  static const base::TimeDelta kDefaultParserBudgetValue =
      GetDefaultParserBudget();
  return kDefaultParserBudgetValue;
}

base::TimeDelta GetTimedBudget(int times_yielded) {
  // Cache the value to avoid parsing the param string more than once.
  static const int kNumYieldsWithDefaultBudgetValue =
      GetNumYieldsWithDefaultBudget();

  // Cache the value to avoid parsing the param string more than once.
  static const base::TimeDelta kLongParserBudgetValue = GetLongParserBudget();

  if (times_yielded <= kNumYieldsWithDefaultBudgetValue) {
    return GetDefaultTimedBudget();
  }
  return kLongParserBudgetValue;
}

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

HTMLDocumentParserState::HTMLDocumentParserState(
    ParserSynchronizationPolicy mode,
    int budget)
    : state_(DeferredParserState::kNotScheduled),
      meta_csp_state_(MetaCSPTokenState::kNotSeen),
      mode_(mode),
      preload_processing_mode_(GetPreloadProcessingMode()),
      budget_(budget) {}

// Wrap pending preloads in a thread safe and ref-counted object since the
// vector is added to from a background thread and taken from from the main
// thread.
class HTMLDocumentParser::PendingPreloads
    : public ThreadSafeRefCounted<PendingPreloads> {
 public:
  PendingPreloads() = default;

  Vector<std::unique_ptr<PendingPreloadData>> Take() {
    base::AutoLock auto_lock(lock_);
    return std::move(preloads_);
  }

  // Returns the number of items pending preload after `preload_data` has been
  // added.
  size_t Add(std::unique_ptr<PendingPreloadData> preload_data) {
    base::AutoLock auto_lock(lock_);
    preloads_.push_back(std::move(preload_data));
    return preloads_.size();
  }

  bool IsEmpty() {
    base::AutoLock auto_lock(lock_);
    return preloads_.empty();
  }

 private:
  base::Lock lock_;
  Vector<std::unique_ptr<PendingPreloadData>> preloads_ GUARDED_BY(lock_);
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

  // For now document fragment parsing never reports errors.
  bool report_errors = false;
  tokenizer_.SetState(TokenizerStateForContextElement(context_element,
                                                      report_errors, options_));

  // No script_runner_ in fragment parser.
  tree_builder_ = MakeGarbageCollected<HTMLTreeBuilder>(
      this, fragment, context_element, parser_content_policy, options_,
      include_shadow_roots);
}

HTMLDocumentParser::HTMLDocumentParser(Document& document,
                                       ParserContentPolicy content_policy,
                                       ParserSynchronizationPolicy sync_policy,
                                       ParserPrefetchPolicy prefetch_policy)
    : ScriptableDocumentParser(document, content_policy),
      options_(&document),
      tokenizer_(options_),
      loading_task_runner_(sync_policy == kForceSynchronousParsing
                               ? nullptr
                               : document.GetTaskRunner(TaskType::kNetworking)),
      task_runner_state_(MakeGarbageCollected<HTMLDocumentParserState>(
          sync_policy,
          // Parser yields in chrome-extension:// or file:// documents can
          // cause UI flickering. To mitigate, use_infinite_budget will
          // parse all the way up to the mojo limit.
          (document.Url().ProtocolIs("chrome-extension") ||
           document.Url().IsLocalFile())
              ? kInfiniteTokenizationBudget
              : kDefaultMaxTokenizationBudget)),
      pending_preloads_(base::MakeRefCounted<PendingPreloads>()),
      scheduler_(sync_policy == kAllowDeferredParsing
                     ? Thread::Current()->Scheduler()
                     : nullptr) {
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::HTMLDocumentParser",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_OUT);
  // Make sure the preload scanner thread will be ready when needed.
  if (ThreadedPreloadScannerEnabled() && !task_runner_state_->IsSynchronous())
    GetPreloadScannerThread();

  // Report metrics for async document parsing or forced synchronous parsing.
  // The document must be outermost main frame to meet UKM requirements, and
  // must have a high resolution clock for high quality data. Additionally, only
  // report metrics for http urls, which excludes things such as the ntp.
  if (sync_policy == kAllowDeferredParsing &&
      document.IsInOutermostMainFrame() &&
      base::TimeTicks::IsHighResolution() &&
      document.Url().ProtocolIsInHTTPFamily()) {
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

  should_skip_preload_scan_ = ShouldSkipPreloadScan();
}

HTMLDocumentParser::~HTMLDocumentParser() {
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::~HTMLDocumentParser",
                         TRACE_ID_LOCAL(this), TRACE_EVENT_FLAG_FLOW_IN);
}

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

unsigned HTMLDocumentParser::GetChunkCountForTesting() const {
  // If `metrics_reporter_` is not set, chunk count is not tracked.
  DCHECK(metrics_reporter_);
  return metrics_reporter_->chunk_count();
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
  background_script_scanner_.Reset();
  background_scanner_.reset();
  tokenizer_.Reset();
}

void HTMLDocumentParser::StopParsing() {
  DocumentParser::StopParsing();
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kNotScheduled);
}

// This kicks off "Once the user agent stops parsing" as described by:
// http://www.whatwg.org/specs/web-apps/current-work/multipage/the-end.html#the-end
void HTMLDocumentParser::PrepareToStopParsing() {
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::PrepareToStopParsing",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);
  base::ElapsedTimer timer;
  DCHECK(!HasInsertionPoint());

  // If we've already been detached, e.g. in
  // WebFrameTest.SwapMainFrameWhileLoading, bail out.
  if (IsDetached())
    return;

  // NOTE: This pump should only ever emit buffered character tokens.
  if (!GetDocument()->IsPrefetchOnly()) {
    ShouldCompleteScope should_complete(task_runner_state_);
    EndIfDelayedForbiddenScope should_not_end_if_delayed(task_runner_state_);
    PumpTokenizerIfPossible();
  }

  if (base::FeatureList::IsEnabled(features::kDelayAsyncScriptExecution) &&
      features::kDelayAsyncScriptExecutionWhenLcpFoundInHtml.Get()) {
    // If kDelayAsyncScriptExecutionWhenLcpFoundInHtml flag is turned on, and an
    // LCP element wasn't found during Preload scan, there is no need to delay
    // async scripts further.
    if (!GetDocument()->IsLcpElementFoundInHtml()) {
      GetDocument()->ResumeAsyncScriptExecution();
    }
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

  GetDocument()->OnPrepareToStopParsing();

  AttemptToRunDeferredScriptsAndEnd();

  base::UmaHistogramTimes("Blink.PrepareToStopParsingTime", timer.Elapsed());
  if (metrics_reporter_) {
    metrics_reporter_->AddPrepareToStopParsingTime(
        timer.Elapsed().InMicroseconds());
  }
}

bool HTMLDocumentParser::IsParsingFragment() const {
  return tree_builder_->IsParsingFragment();
}

void HTMLDocumentParser::DeferredPumpTokenizerIfPossible(
    bool from_finish_append,
    base::TimeTicks schedule_time) {
  // This method is called asynchronously, continues building the HTML document.

  // If we're scheduled for a tokenizer pump, then document should be attached
  // and the parser should not be stopped, but sometimes a script completes
  // loading (so we schedule a pump) but the Document is stopped in the meantime
  // (e.g. fast/parser/iframe-onload-document-close-with-external-script.html).
  DCHECK(task_runner_state_->GetState() ==
             HTMLDocumentParserState::DeferredParserState::kNotScheduled ||
         !IsDetached());
  TRACE_EVENT_WITH_FLOW2(
      "blink", "HTMLDocumentParser::DeferredPumpTokenizerIfPossible",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "parser",
      (void*)this, "state", task_runner_state_->GetStateAsString());

  if (metrics_reporter_ && from_finish_append && !did_pump_tokenizer_) {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Blink.HTMLParsing.TimeToDeferredPumpTokenizer4",
        base::TimeTicks::Now() - schedule_time, base::Microseconds(1),
        base::Seconds(1), 100);
  }

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
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::PumpTokenizerIfPossible",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);

  bool yielded = false;
  CheckIfBlockingStylesheetAdded();
  if (!IsStopped() &&
      (!IsPaused() || task_runner_state_->ShouldEndIfDelayed())) {
    yielded = PumpTokenizer();
  }

  if (yielded) {
    DCHECK(!task_runner_state_->ShouldComplete());
    SchedulePumpTokenizer(/*from_finish_append=*/false);
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
  TRACE_EVENT_WITH_FLOW1("blink",
                         "HTMLDocumentParser::RunScriptsForPausedTreeBuilder",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
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

void HTMLDocumentParser::ForcePlaintextForTextDocument() {
  tokenizer_.SetState(HTMLTokenizer::kPLAINTEXTState);
}

bool HTMLDocumentParser::PumpTokenizer() {
  DCHECK(!GetDocument()->IsPrefetchOnly());
  DCHECK(!IsStopped());

  did_pump_tokenizer_ = true;

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
  base::ElapsedTimer pump_tokenizer_timer;

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
  int budget =
      (task_runner_state_->TimesYielded() <= kNumYieldsWithDefaultBudget)
          ? task_runner_state_->GetDefaultBudget()
          : kInfiniteTokenizationBudget;

  if (RuntimeEnabledFeatures::HTMLParserYieldAndDelayOftenForTestingEnabled()) {
    budget = 2;
  }

  base::TimeDelta timed_budget;
  if (TimedParserBudgetEnabled())
    timed_budget = GetTimedBudget(task_runner_state_->TimesYielded());

  const bool should_process_preloading =
      task_runner_state_->ShouldProcessPreloads();
  base::ElapsedTimer chunk_parsing_timer;
  base::TimeDelta elapsed_time;
  unsigned tokens_parsed = 0;
  int characters_consumed_before_token = 0;
  base::TimeDelta time_executing_script;
  v8::Isolate* isolate = GetDocument()->GetAgent().isolate();
  while (true) {
    if (should_process_preloading)
      FlushPendingPreloads();

    const auto next_token_status = CanTakeNextToken(time_executing_script);
    if (next_token_status == kNoTokens) {
      // No tokens left to process in this pump, so break
      break;
    }
    if (next_token_status == kHaveTokensAfterScript &&
        task_runner_state_->HaveExitedHeader()) {
      // Just executed a parser-blocking script in the body. We'd probably like
      // to yield at some point soon, especially if we're in "extended budget"
      // mode. So reduce the budget back to at most the default.
      budget = std::min(budget, task_runner_state_->GetDefaultBudget());
      if (TimedParserBudgetEnabled()) {
        timed_budget = std::min(timed_budget, chunk_parsing_timer.Elapsed() +
                                                  GetDefaultTimedBudget());
      }
    }
    HTMLToken* token;
    {
      RUNTIME_CALL_TIMER_SCOPE(
          isolate, RuntimeCallStats::CounterId::kHTMLTokenizerNextToken);
      token = tokenizer_.NextToken(input_.Current());
      if (!token)
        break;
      budget--;
      tokens_parsed++;
    }
    AtomicHTMLToken atomic_html_token(*token);
    // Clear the HTMLToken in case ConstructTree() synchronously re-enters the
    // parser. This has to happen after creating AtomicHTMLToken as it needs
    // state in the HTMLToken.
    tokenizer_.ClearToken();
    ConstructTreeFromToken(atomic_html_token);
    if (!should_run_until_completion && !IsPaused()) {
      DCHECK_EQ(task_runner_state_->GetMode(), kAllowDeferredParsing);
      if (TimedParserBudgetEnabled() &&
          !RuntimeEnabledFeatures::
              HTMLParserYieldAndDelayOftenForTestingEnabled()) {
        if (CheckParserBudgetLessOften()) {
          int newly_consumed_characters =
              input_.Current().NumberOfCharactersConsumed() -
              characters_consumed_before_token;
          characters_consumed_before_token =
              input_.Current().NumberOfCharactersConsumed();
          // On android calling chunk_parsing_timer.Elapsed seems fairly slow
          // compared to the parsing time of small tokens. Only update the
          // timer occasionally.
          if (ShouldCheckTimeBudget(next_token_status,
                                    atomic_html_token.GetHTMLTag(),
                                    newly_consumed_characters, tokens_parsed)) {
            elapsed_time = chunk_parsing_timer.Elapsed();
          }
        } else {
          elapsed_time = chunk_parsing_timer.Elapsed();
        }
        should_yield = elapsed_time >= timed_budget;
      } else {
        should_yield = budget <= 0;
      }
      should_yield |= scheduler_->ShouldYieldForHighPriorityWork();
      should_yield &= task_runner_state_->HaveExitedHeader();

      // Yield for preloads even if we haven't exited the header, since they
      // should be dispatched as soon as possible.
      if (task_runner_state_->ShouldYieldForPreloads())
        should_yield |= HasPendingPreloads();
      if (should_yield)
        break;
    }
  }

  base::UmaHistogramTimes("Blink.PumpTokenizerTime",
                          pump_tokenizer_timer.Elapsed());
  if (metrics_reporter_) {
    metrics_reporter_->AddPumpTokenizerTime(
        pump_tokenizer_timer.Elapsed().InMicroseconds());
  }

  if (is_tracing) {
    TRACE_EVENT_END2("blink", "HTMLDocumentParser::PumpTokenizer",
                     "parsed_tokens", tokens_parsed, "parsed_bytes",
                     starting_bytes - input_.length());
  }

  const bool is_stopped_or_parsing_fragment =
      IsStopped() || IsParsingFragment();

  if (!is_stopped_or_parsing_fragment) {
    // There should only be PendingText left since the tree-builder always
    // flushes the task queue before returning. In case that ever changes,
    // crash.
    tree_builder_->Flush();
    CHECK(!IsStopped());
  }

  if (tokens_parsed && metrics_reporter_) {
    metrics_reporter_->AddChunk(
        chunk_parsing_timer.Elapsed() - time_executing_script, tokens_parsed);
  }

  if (is_stopped_or_parsing_fragment)
    return false;

  if (IsPaused() && preloader_ && !background_scanner_ &&
      !should_skip_preload_scan_) {
    if (!preload_scanner_) {
      preload_scanner_ =
          CreatePreloadScanner(TokenPreloadScanner::ScannerType::kMainDocument);
      preload_scanner_->AppendToEnd(input_.Current());
    }
    ScanAndPreload(preload_scanner_.get());
  }

  // should_run_until_completion implies that we should not yield
  CHECK(!should_run_until_completion || !should_yield);
  if (should_yield)
    task_runner_state_->MarkYield();

  return should_yield;
}

void HTMLDocumentParser::SchedulePumpTokenizer(bool from_finish_append) {
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::SchedulePumpTokenizer",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!IsStopped());
  DCHECK(!task_runner_state_->InPumpSession());
  DCHECK(!task_runner_state_->ShouldComplete());
  if (task_runner_state_->IsScheduled()) {
    // If the parser is already scheduled, there's no need to do anything.
    return;
  }
  base::TimeDelta delay = base::Milliseconds(0);
  if (RuntimeEnabledFeatures::HTMLParserYieldAndDelayOftenForTestingEnabled()) {
    delay = base::Milliseconds(10);
  }
  loading_task_runner_->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(&HTMLDocumentParser::DeferredPumpTokenizerIfPossible,
                    WrapPersistent(this), from_finish_append,
                    base::TimeTicks::Now()),
      delay);
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kScheduled);

  yield_timer_ = std::make_unique<base::ElapsedTimer>();
}

void HTMLDocumentParser::ScheduleEndIfDelayed() {
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::ScheduleEndIfDelayed",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  DCHECK(!IsStopped());
  DCHECK(!task_runner_state_->InPumpSession());
  DCHECK(!task_runner_state_->ShouldComplete());

  // Schedule a pump callback if needed.
  if (!task_runner_state_->IsScheduled()) {
    loading_task_runner_->PostTask(
        FROM_HERE,
        WTF::BindOnce(&HTMLDocumentParser::DeferredPumpTokenizerIfPossible,
                      WrapPersistent(this),
                      /*from_finish_append=*/false, base::TimeTicks::Now()));
    yield_timer_ = std::make_unique<base::ElapsedTimer>();
  }
  // If a pump is already scheduled, it's OK to just upgrade it to one
  // which calls EndIfDelayed afterwards.
  task_runner_state_->SetState(
      HTMLDocumentParserState::DeferredParserState::kScheduledWithEndIfDelayed);
}

void HTMLDocumentParser::ConstructTreeFromToken(AtomicHTMLToken& atomic_token) {
  DCHECK(!GetDocument()->IsPrefetchOnly());

  // Check whether we've exited the header.
  if (!task_runner_state_->HaveExitedHeader()) {
    if (GetDocument()->body()) {
      task_runner_state_->SetExitedHeader();
    }
  }

  tree_builder_->ConstructTree(&atomic_token);
  CheckIfBlockingStylesheetAdded();
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
  // No need to do any processing if the supplied text is empty.
  if (IsStopped() || source.empty())
    return;

  TRACE_EVENT_WITH_FLOW2(
      "blink", "HTMLDocumentParser::insert", TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "source_length",
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

  if (IsPaused() && !should_skip_preload_scan_) {
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
  TRACE_EVENT_WITH_FLOW2("blink", "HTMLDocumentParser::append",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "size", input_source.length(), "parser", (void*)this);

  if (IsStopped())
    return;

  const SegmentedString source(input_source);

  ScanInBackground(input_source);

  if (!background_scanner_ && !preload_scanner_ && preloader_ &&
      GetDocument()->Url().IsValid() && !should_skip_preload_scan_ &&
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
    if (preload_scanner_) {
      preload_scanner_->AppendToEnd(source);
      // TODO(Richard.Townsend@arm.com): add test coverage of this branch.
      // The crash in crbug.com/1166786 indicates that text documents are being
      // speculatively prefetched.
      ScanAndPreload(preload_scanner_.get());
    }

    // Return after the preload scanner, do not actually parse the document.
    return;
  }
  if (preload_scanner_) {
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
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::FinishAppend",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (ShouldPumpTokenizerNowForFinishAppend())
    PumpTokenizerIfPossible();
  else
    SchedulePumpTokenizer(/*from_finish_append=*/true);
}

void HTMLDocumentParser::CommitPreloadedData() {
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::CommitPreloadedData",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
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
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::AttemptToEnd",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);
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
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::EndIfDelayed",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);
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
  if (!input_.HaveSeenEndOfFile()) {
    input_.MarkEndOfFile();
  }

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
    DCHECK(!(tree_builder_->HasParserBlockingScript() ||
             (script_runner_ && script_runner_->HasParserBlockingScript()) ||
             reentry_permit_->ParserPauseFlag()));
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
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::ResumeParsingAfterPause",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);
  DCHECK(!IsExecutingScript());
  DCHECK(!IsPaused());

  CheckIfBlockingStylesheetAdded();
  if (IsStopped() || IsPaused() || IsDetached())
    return;

  insertion_preload_scanner_.reset();
  if (task_runner_state_->GetMode() == kAllowDeferredParsing &&
      !task_runner_state_->ShouldComplete() &&
      !task_runner_state_->InPumpSession()) {
    SchedulePumpTokenizer(/*from_finish_append=*/false);
  } else {
    ShouldCompleteScope should_complete(task_runner_state_);
    PumpTokenizerIfPossible();
  }
}

void HTMLDocumentParser::AppendCurrentInputStreamToPreloadScannerAndScan() {
  TRACE_EVENT_WITH_FLOW1(
      "blink",
      "HTMLDocumentParser::AppendCurrentInputStreamToPreloadScannerAndScan",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "parser",
      (void*)this);
  if (preload_scanner_) {
    DCHECK(preloader_);
    preload_scanner_->AppendToEnd(input_.Current());
    ScanAndPreload(preload_scanner_.get());
  }
}

void HTMLDocumentParser::NotifyScriptLoaded() {
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::NotifyScriptLoaded",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);
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

// static
void HTMLDocumentParser::ResetCachedFeaturesForTesting() {
  ThreadedPreloadScannerEnabled(FeatureResetMode::kResetForTesting);
  PrecompileInlineScriptsEnabled(FeatureResetMode::kResetForTesting);
}

// static
void HTMLDocumentParser::FlushPreloadScannerThreadForTesting() {
  base::RunLoop run_loop;
  GetPreloadScannerThread()->GetTaskRunner()->PostTask(FROM_HERE,
                                                       run_loop.QuitClosure());
  run_loop.Run();
}

void HTMLDocumentParser::ExecuteScriptsWaitingForResources() {
  TRACE_EVENT_WITH_FLOW0(
      "blink", "HTMLDocumentParser::ExecuteScriptsWaitingForResources",
      TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  if (IsStopped())
    return;

  DCHECK(GetDocument()->IsScriptExecutionReady());

  if (task_runner_state_->WaitingForStylesheets())
    task_runner_state_->SetWaitingForStylesheets(false);

  if (IsStopping()) {
    AttemptToRunDeferredScriptsAndEnd();
    return;
  }

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

  if (RuntimeEnabledFeatures::DOMPartsAPIEnabled()) {
    // Within templates containing the `parseparts` attribute, allow parsing
    // DOM Parts. Otherwise do not parse any DOM Part content.
    DOMPartsAllowed parts_allowed{DOMPartsAllowed::kNever};
    if (auto* template_element =
            DynamicTo<HTMLTemplateElement>(context_element);
        template_element &&
        template_element->hasAttribute(html_names::kParsepartsAttr)) {
      parts_allowed = DOMPartsAllowed::kAlways;
    }
    parser->tree_builder_->SetDOMPartsAllowedState(parts_allowed);
  }

  parser->Append(source);
  parser->Finish();
  // Allows ~DocumentParser to assert it was detached before destruction.
  parser->Detach();
}

void HTMLDocumentParser::AppendBytes(base::span<const uint8_t> data) {
  TRACE_EVENT_WITH_FLOW2(
      "blink", "HTMLDocumentParser::appendBytes", TRACE_ID_LOCAL(this),
      TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT, "size",
      static_cast<unsigned>(data.size()), "parser", (void*)this);

  DCHECK(IsMainThread());

  if (data.empty() || IsStopped()) {
    return;
  }

  DecodedDataDocumentParser::AppendBytes(data);
}

void HTMLDocumentParser::Flush() {
  TRACE_EVENT_WITH_FLOW1("blink", "HTMLDocumentParser::Flush",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT,
                         "parser", (void*)this);
  // If we've got no decoder, we never received any data.
  if (IsDetached() || NeedsDecoder())
    return;
  DecodedDataDocumentParser::Flush();
}

void HTMLDocumentParser::SetDecoder(
    std::unique_ptr<TextResourceDecoder> decoder) {
  DecodedDataDocumentParser::SetDecoder(std::move(decoder));
}

void HTMLDocumentParser::DocumentElementAvailable() {
  TRACE_EVENT_WITH_FLOW0("blink,loading",
                         "HTMLDocumentParser::DocumentElementAvailable",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  Document* document = GetDocument();
  DCHECK(document);
  DCHECK(document->documentElement());
  Element* documentElement = GetDocument()->documentElement();
  if (documentElement->hasAttribute(AtomicString(u"\u26A1")) ||
      documentElement->hasAttribute(AtomicString("amp")) ||
      documentElement->hasAttribute(AtomicString("i-amphtml-layout"))) {
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
#if DCHECK_IS_ON()
  if (scanner_type == TokenPreloadScanner::ScannerType::kMainDocument) {
    // A main document scanner should never be created if scanning is already
    // happening in the background.
    DCHECK(!background_scanner_);
    // If background scanning is enabled, the main document scanner is used when
    // the parser is paused, for prefetch documents, or if preload scanning is
    // disabled in tests (HTMLPreloadScanner internally handles this setting).
    DCHECK(!ThreadedPreloadScannerEnabled() || IsPaused() ||
           GetDocument()->IsPrefetchOnly() ||
           !IsPreloadScanningEnabled(GetDocument()));
  }
#endif
  return HTMLPreloadScanner::Create(*GetDocument(), options_, scanner_type);
}

void HTMLDocumentParser::ScanAndPreload(HTMLPreloadScanner* scanner) {
  TRACE_EVENT_WITH_FLOW0("blink", "HTMLDocumentParser::ScanAndPreload",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
  CHECK(preloader_);
  base::ElapsedTimer timer_before_scan;
  std::unique_ptr<PendingPreloadData> preload_data =
      scanner->Scan(GetDocument()->ValidBaseElementURL());
  const base::TimeDelta scan_time = timer_before_scan.Elapsed();
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat(
          {kHistogramScanAndPreloadTime, ".Scan", GetPreloadHistogramSuffix()}),
      scan_time);
  base::ElapsedTimer timer_after_scan;
  ProcessPreloadData(std::move(preload_data));
  const base::TimeDelta scan_and_preload_time = timer_before_scan.Elapsed();
  const base::TimeDelta preload_time = timer_after_scan.Elapsed();
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kHistogramScanAndPreloadTime, GetPreloadHistogramSuffix()}),
      scan_and_preload_time);
  // Keep old histogram until next expiry date.
  base::UmaHistogramTimes(
      base::StrCat({"Blink.ScanAndPreloadTime", GetPreloadHistogramSuffix()}),
      scan_and_preload_time);
  base::UmaHistogramMicrosecondsTimes(
      base::StrCat({kHistogramScanAndPreloadTime, ".Preload",
                    GetPreloadHistogramSuffix()}),
      preload_time);
  if (metrics_reporter_) {
    metrics_reporter_->AddScanTime(scan_time.InMicroseconds());
    metrics_reporter_->AddScanAndPreloadTime(
        scan_and_preload_time.InMicroseconds());
    metrics_reporter_->AddPreloadTime(preload_time.InMicroseconds());
  }
}

void HTMLDocumentParser::ProcessPreloadData(
    std::unique_ptr<PendingPreloadData> preload_data) {
  for (const auto& value : preload_data->meta_ch_values) {
    HTMLMetaElement::ProcessMetaCH(*GetDocument(), value.value, value.type,
                                   value.is_doc_preloader,
                                   /*is_sync_parser=*/false);
  }

  // Make sure that the viewport is up-to-date, so that the correct viewport
  // dimensions will be fed to the preload scanner.
  if (GetDocument()->Loader() &&
      task_runner_state_->GetMode() == kAllowDeferredParsing) {
    if (preload_data->viewport.has_value()) {
      GetDocument()->GetStyleEngine().UpdateViewport();
    }
    if (task_runner_state_->NeedsLinkHeaderPreloadsDispatch()) {
      {
        TRACE_EVENT_WITH_FLOW0(
            "blink", "HTMLDocumentParser::DispatchLinkHeaderPreloads",
            TRACE_ID_LOCAL(this),
            TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
        GetDocument()->Loader()->DispatchLinkHeaderPreloads(
            base::OptionalToPtr(preload_data->viewport),
            PreloadHelper::LoadLinksFromHeaderMode::
                kDocumentAfterCommitWithViewport);
      }
      if (base::FeatureList::IsEnabled(
              blink::features::kLCPPFontURLPredictor)) {
        TRACE_EVENT_WITH_FLOW0(
            "blink", "HTMLDocumentParser::DispatchLcppFontPreloads",
            TRACE_ID_LOCAL(this),
            TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
        GetDocument()->Loader()->DispatchLcppFontPreloads(
            base::OptionalToPtr(preload_data->viewport),
            PreloadHelper::LoadLinksFromHeaderMode::
                kSubresourceNotFromMemoryCache);
      }
      if (GetDocument()->Loader()->GetPrefetchedSignedExchangeManager()) {
        TRACE_EVENT_WITH_FLOW0(
            "blink", "HTMLDocumentParser::DispatchSignedExchangeManager",
            TRACE_ID_LOCAL(this),
            TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);
        // Link header preloads for prefetched signed exchanges won't be started
        // until StartPrefetchedLinkHeaderPreloads() is called. See the header
        // comment of PrefetchedSignedExchangeManager.
        GetDocument()
            ->Loader()
            ->GetPrefetchedSignedExchangeManager()
            ->StartPrefetchedLinkHeaderPreloads();
      }
      task_runner_state_->DispatchedLinkHeaderPreloads();
    }
  }

  task_runner_state_->SetSeenCSPMetaTag(preload_data->has_csp_meta_tag);
  for (auto& request : preload_data->requests) {
    queued_preloads_.push_back(std::move(request));
    if (metrics_reporter_) {
      metrics_reporter_->IncrementPreloadRequestCount();
    }
  }

  if (base::FeatureList::IsEnabled(features::kDelayAsyncScriptExecution) &&
      features::kDelayAsyncScriptExecutionWhenLcpFoundInHtml.Get()) {
    // If LCP element is found during preload scanning of main document,
    // start/continue delaying async script execution on the document until the
    // configured  milestone.
    Document* document = GetDocument();
    LocalFrame* frame = document->GetFrame();
    if (preload_data->has_located_potential_lcp_element && frame &&
        frame->IsMainFrame()) {
      document->SetLcpElementFoundInHtml(true);
      document->DelayAsyncScriptExecution();
    }
  }

  if (base::FeatureList::IsEnabled(
          features::kLowPriorityAsyncScriptExecution) &&
      features::kLowPriorityAsyncScriptExecutionDisableWhenLcpNotInHtmlParam
          .Get()) {
    // Update main document whether LCP element was discovered in HTML.
    Document* document = GetDocument();
    LocalFrame* frame = document->GetFrame();
    if (preload_data->has_located_potential_lcp_element && frame &&
        frame->IsMainFrame()) {
      document->SetLcpElementFoundInHtml(true);
    }
  }

  FetchQueuedPreloads();
}

void HTMLDocumentParser::FetchQueuedPreloads() {
  DCHECK(preloader_);
  TRACE_EVENT_WITH_FLOW0("blink,devtools.timeline",
                         "HTMLDocumentParser::FetchQueuedPreloads",
                         TRACE_ID_LOCAL(this),
                         TRACE_EVENT_FLAG_FLOW_IN | TRACE_EVENT_FLAG_FLOW_OUT);

  if (!queued_preloads_.empty()) {
    base::ElapsedTimer timer;
    preloader_->TakeAndPreload(queued_preloads_);
    base::UmaHistogramTimes(base::StrCat({"Blink.FetchQueuedPreloadsTime",
                                          GetPreloadHistogramSuffix()}),
                            timer.Elapsed());
    if (metrics_reporter_) {
      metrics_reporter_->AddFetchQueuedPreloadsTime(
          timer.Elapsed().InMicroseconds());
    }
  }
}

std::string HTMLDocumentParser::GetPreloadHistogramSuffix() {
  bool is_outermost_main_frame =
      GetDocument() && GetDocument()->IsInOutermostMainFrame();
  bool have_seen_first_byte = task_runner_state_->SeenFirstByte();
  return base::StrCat({is_outermost_main_frame ? ".MainFrame" : ".Subframe",
                       have_seen_first_byte ? ".NonInitial" : ".Initial"});
}

DocumentParser::BackgroundScanCallback
HTMLDocumentParser::TakeBackgroundScanCallback() {
  if (!background_scan_fn_)
    return BackgroundScanCallback();
  return CrossThreadBindRepeating(std::move(background_scan_fn_), KURL());
}

void HTMLDocumentParser::ScanInBackground(const String& source) {
  if (task_runner_state_->IsSynchronous() || !GetDocument()->Url().IsValid())
    return;

  if (ThreadedPreloadScannerEnabled() && preloader_ &&
      // TODO(crbug.com/1329535): Support scanning prefetch documents in the
      // background.
      !GetDocument()->IsPrefetchOnly() &&
      IsPreloadScanningEnabled(GetDocument()) && !should_skip_preload_scan_) {
    // The background scanner should never be created if a main thread scanner
    // is already available.
    DCHECK(!preload_scanner_);
    if (!background_scanner_) {
      // See comment on NavigationBodyLoader::StartLoadingBodyInBackground() for
      // details on how the preload scanner flow works when the body data is
      // being loaded in the background.
      background_scanner_ = HTMLPreloadScanner::CreateBackground(
          this, options_, GetPreloadScannerThread()->GetTaskRunner(),
          CrossThreadBindRepeating(
              &HTMLDocumentParser::AddPreloadDataOnBackgroundThread,
              MakeCrossThreadWeakHandle(this), this->pending_preloads_,
              GetDocument()->GetTaskRunner(TaskType::kInternalLoading)));

      background_scan_fn_ = CrossThreadBindRepeating(
          [](base::WeakPtr<HTMLPreloadScanner> scanner,
             scoped_refptr<base::SingleThreadTaskRunner> task_runner,
             const KURL& url, const String& data) {
            PostCrossThreadTask(
                *task_runner, FROM_HERE,
                CrossThreadBindOnce(&HTMLPreloadScanner::ScanInBackground,
                                    std::move(scanner), data, url));
          },
          background_scanner_->AsWeakPtr(),
          GetPreloadScannerThread()->GetTaskRunner());
    }

    if (background_scan_fn_)
      background_scan_fn_.Run(GetDocument()->ValidBaseElementURL(), source);
    return;
  }

  if (!PrecompileInlineScriptsEnabled()) {
    return;
  }

  DCHECK(!background_scanner_);
  if (!background_script_scanner_)
    background_script_scanner_ = BackgroundHTMLScanner::Create(options_, this);

  if (background_script_scanner_) {
    background_script_scanner_.AsyncCall(&BackgroundHTMLScanner::Scan)
        .WithArgs(source);
  }
}

// static
void HTMLDocumentParser::AddPreloadDataOnBackgroundThread(
    CrossThreadWeakHandle<HTMLDocumentParser> parser_handle,
    scoped_refptr<PendingPreloads> pending_preloads,
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    std::unique_ptr<PendingPreloadData> preload_data) {
  DCHECK(!IsMainThread());

  size_t num_pending_preloads = pending_preloads->Add(std::move(preload_data));

  // Only post a task if the preload data was empty before we added this data.
  // Otherwise, a task has already been posted and will consume the new data.
  if (num_pending_preloads == 1) {
    PostCrossThreadTask(
        *task_runner, FROM_HERE,
        CrossThreadBindOnce(
            &HTMLDocumentParser::FlushPendingPreloads,
            MakeUnwrappingCrossThreadWeakHandle(std::move(parser_handle))));
  }
}

bool HTMLDocumentParser::HasPendingPreloads() {
  return pending_preloads_->IsEmpty();
}

void HTMLDocumentParser::FlushPendingPreloads() {
  DCHECK(IsMainThread());
  if (!ThreadedPreloadScannerEnabled())
    return;

  if (IsDetached() || !preloader_)
    return;

  // Do this in a loop in case more preloads are added in the background.
  Vector<std::unique_ptr<PendingPreloadData>> preload_data;
  while (!(preload_data = pending_preloads_->Take()).empty()) {
    for (auto& preload : preload_data) {
      ProcessPreloadData(std::move(preload));
    }
  }
}

bool HTMLDocumentParser::ShouldPumpTokenizerNowForFinishAppend() const {
  if (task_runner_state_->GetMode() !=
          ParserSynchronizationPolicy::kAllowDeferredParsing ||
      task_runner_state_->ShouldComplete()) {
    return true;
  }
  if (!base::FeatureList::IsEnabled(features::kProcessHtmlDataImmediately))
    return false;

  // When a debugger is attached a nested message loop may be created during
  // commit. Processing the data now can lead to unexpected states.
  // TODO(https://crbug.com/1364695): see if this limitation can be removed.
  if (auto* sink = probe::ToCoreProbeSink(GetDocument())) {
    if (sink->HasAgentsGlobal(CoreProbeSink::kDevToolsSession))
      return false;
  }

  if (GetDocument()->IsInOutermostMainFrame()) {
    if (!features::kProcessHtmlDataImmediatelyMainFrame.Get())
      return false;
  } else if (!features::kProcessHtmlDataImmediatelyChildFrame.Get()) {
    return false;
  }

  return did_pump_tokenizer_
             ? features::kProcessHtmlDataImmediatelySubsequentChunks.Get()
             : features::kProcessHtmlDataImmediatelyFirstChunk.Get();
}

ALWAYS_INLINE bool HTMLDocumentParser::ShouldCheckTimeBudget(
    NextTokenStatus next_token_status,
    html_names::HTMLTag tag,
    int newly_consumed_characters,
    int tokens_parsed) const {
  if (next_token_status == kHaveTokensAfterScript) {
    // If we executed a script when parsing this token, then check the time
    // budget again since script execution is slow.
    return true;
  }
  if (newly_consumed_characters > 200) {
    // Always update timer on tokens of more than 200 characters as they're
    // often slow.
    return true;
  }

  // <style>, <iframe> and <link> tags are slow to parse.
  if (tag == html_names::HTMLTag::kStyle ||
      tag == html_names::HTMLTag::kIFrame ||
      tag == html_names::HTMLTag::kLink) {
    return true;
  }

  // The token is probably fast to parse, only update the timer for 10% of
  // those tokens.
  return tokens_parsed % 10 == 0;
}

bool HTMLDocumentParser::ShouldSkipPreloadScan() {
  // Check if Document-Policy has Expect-No-Linked-Resources hint.
  auto* document = GetDocument();
  if (const auto* context = document->GetExecutionContext()) {
    if (context->IsFeatureEnabled(
            mojom::blink::DocumentPolicyFeature::kExpectNoLinkedResources)) {
      UseCounter::Count(document,
                        WebFeature::kDocumentPolicyExpectNoLinkedResources);
      return true;
    }
  }

  return false;
}

}  // namespace blink
