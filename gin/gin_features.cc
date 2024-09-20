// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/gin_features.h"

#include "base/metrics/field_trial_params.h"

namespace features {

// Enable code space compaction when finalizing a full GC with stack.
BASE_FEATURE(kV8CompactCodeSpaceWithStack,
             ("V8CompactCodeSpaceWithStack"),
             kFeatureDefaultStateControlledByV8);

// Enable compaction when finalizing a full GC with stack.
BASE_FEATURE(kV8CompactWithStack,
             ("V8CompactWithStack"),
             kFeatureDefaultStateControlledByV8);

// Decommit (rather than discard) pooled pages.
BASE_FEATURE(kV8DecommitPooledPages,
             ("DecommitPooledPages"),
             kFeatureDefaultStateControlledByV8);

// Enables optimization of JavaScript in V8.
BASE_FEATURE(kV8OptimizeJavascript,
             ("V8OptimizeJavascript"),
             kFeatureDefaultStateControlledByV8);

// Enables flushing of JS bytecode in V8.
BASE_FEATURE(kV8FlushBytecode,
             ("V8FlushBytecode"),
             kFeatureDefaultStateControlledByV8);
const base::FeatureParam<int> kV8FlushBytecodeOldAge{
    &kV8FlushBytecode, "V8FlushBytecodeOldAge", 5};

// Enables flushing of baseline code in V8.
BASE_FEATURE(kV8FlushBaselineCode,
             ("V8FlushBaselineCode"),
             kFeatureDefaultStateControlledByV8);

// Enables code flushing based on tab visibility.
BASE_FEATURE(kV8FlushCodeBasedOnTabVisibility,
             ("V8FlushCodeBasedOnTabVisibility"),
             kFeatureDefaultStateControlledByV8);

// Enables code flushing based on time.
BASE_FEATURE(kV8FlushCodeBasedOnTime,
             ("V8FlushCodeBasedOnTime"),
             kFeatureDefaultStateControlledByV8);
const base::FeatureParam<int> kV8FlushCodeOldTime{&kV8FlushCodeBasedOnTime,
                                                  "V8FlushCodeOldTime", 30};

// Enables finalizing streaming JS compilations on a background thread.
BASE_FEATURE(kV8OffThreadFinalization,
             ("V8OffThreadFinalization"),
             kFeatureDefaultStateControlledByV8);

// Enables lazy feedback allocation in V8.
BASE_FEATURE(kV8LazyFeedbackAllocation,
             ("V8LazyFeedbackAllocation"),
             kFeatureDefaultStateControlledByV8);

// Enables per-context marking worklists in V8 GC.
BASE_FEATURE(kV8PerContextMarkingWorklist,
             ("V8PerContextMarkingWorklist"),
             kFeatureDefaultStateControlledByV8);

// Enables flushing of the instruction cache for the embedded blob.
BASE_FEATURE(kV8FlushEmbeddedBlobICache,
             ("V8FlushEmbeddedBlobICache"),
             kFeatureDefaultStateControlledByV8);

// Enables reduced number of concurrent marking tasks.
BASE_FEATURE(kV8ReduceConcurrentMarkingTasks,
             ("V8ReduceConcurrentMarkingTasks"),
             kFeatureDefaultStateControlledByV8);

// Disables reclaiming of unmodified wrappers objects.
BASE_FEATURE(kV8NoReclaimUnmodifiedWrappers,
             ("V8NoReclaimUnmodifiedWrappers"),
             kFeatureDefaultStateControlledByV8);

// Enables W^X code memory protection in V8.
// This is enabled in V8 by default. To test the performance impact, we are
// going to disable this feature in a finch experiment.
BASE_FEATURE(kV8CodeMemoryWriteProtection,
             ("V8CodeMemoryWriteProtection"),
             kFeatureDefaultStateControlledByV8);

// Enables fallback to a breadth-first regexp engine on excessive backtracking.
BASE_FEATURE(kV8ExperimentalRegexpEngine,
             "V8ExperimentalRegexpEngine",
             kFeatureDefaultStateControlledByV8);

// Enable accounting for external memory limits as part of global limits in v8
// Heap.
BASE_FEATURE(kV8ExternalMemoryAccountedInGlobalLimit,
             "V8ExternalMemoryAccountedInGlobalLimit",
             kFeatureDefaultStateControlledByV8);

// Enables the Turbofan compiler.
BASE_FEATURE(kV8Turbofan, ("V8Turbofan"), kFeatureDefaultStateControlledByV8);

// Enables Turbofan's new compiler IR Turboshaft.
BASE_FEATURE(kV8Turboshaft,
             ("V8Turboshaft"),
             kFeatureDefaultStateControlledByV8);

// Enable running instruction selection on Turboshaft IR directly.
BASE_FEATURE(kV8TurboshaftInstructionSelection,
             ("V8TurboshaftInstructionSelection"),
             kFeatureDefaultStateControlledByV8);

// Enables Maglev compiler. Note that this only sets the V8 flag when
// manually overridden; otherwise it defers to whatever the V8 default is.
BASE_FEATURE(kV8Maglev, ("V8Maglev"), kFeatureDefaultStateControlledByV8);
BASE_FEATURE(kV8ConcurrentMaglevHighPriorityThreads,
             ("V8ConcurrentMaglevHighPriorityThreads"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8MemoryReducer,
             "V8MemoryReducer",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kV8MemoryReducerGCCount{
    &kV8MemoryReducer, "V8MemoryReducerGCCount", 3};

// Enables MinorMC young generation garbage collector.
BASE_FEATURE(kV8MinorMS, ("V8MinorMS"), kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8ScavengerHigherCapacity,
             "V8ScavengerHigherCapacity",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kV8ScavengerMaxCapacity{
    &kV8ScavengerHigherCapacity, "V8ScavengerMaxCapacity", 16};

BASE_FEATURE(kV8SeparateGCPhases,
             ("V8SeparateGCPhases"),
             kFeatureDefaultStateControlledByV8);

// Enables Sparkplug compiler. Note that this only sets the V8 flag when
// manually overridden; otherwise it defers to whatever the V8 default is.
BASE_FEATURE(kV8Sparkplug, ("V8Sparkplug"), kFeatureDefaultStateControlledByV8);

// Enables the concurrent Sparkplug compiler.
BASE_FEATURE(kV8ConcurrentSparkplug,
             ("V8ConcurrentSparkplug"),
             kFeatureDefaultStateControlledByV8);
const base::FeatureParam<int> kV8ConcurrentSparkplugMaxThreads{
    &kV8ConcurrentSparkplug, "V8ConcurrentSparkplugMaxThreads", 0};
BASE_FEATURE(kV8ConcurrentSparkplugHighPriorityThreads,
             ("V8ConcurrentSparkplugHighPriorityThreads"),
             kFeatureDefaultStateControlledByV8);
// Makes sure the experimental Sparkplug compiler is only enabled if short
// builtin calls are enabled too.
BASE_FEATURE(kV8SparkplugNeedsShortBuiltinCalls,
             ("V8SparkplugNeedsShortBuiltinCalls"),
             kFeatureDefaultStateControlledByV8);

// Enables batch compilation for Sparkplug (baseline) compilation.
BASE_FEATURE(kV8BaselineBatchCompilation,
             ("V8BaselineBatchCompilation"),
             kFeatureDefaultStateControlledByV8);

// Enables short builtin calls feature.
BASE_FEATURE(kV8ShortBuiltinCalls,
             ("V8ShortBuiltinCalls"),
             kFeatureDefaultStateControlledByV8);

// Enables fast API calls in TurboFan.
BASE_FEATURE(kV8TurboFastApiCalls,
             ("V8TurboFastApiCalls"),
             kFeatureDefaultStateControlledByV8);

// Enables faster DOM methods for megamorphic ICs
BASE_FEATURE(kV8MegaDomIC, ("V8MegaDomIC"), kFeatureDefaultStateControlledByV8);

// Faster object cloning
BASE_FEATURE(kV8SideStepTransitions,
             ("V8SideStepTransitions"),
             kFeatureDefaultStateControlledByV8);

// Avoids background threads for GC if isolate is in background.
BASE_FEATURE(kV8SingleThreadedGCInBackground,
             ("V8SingleThreadedGCInBackground"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8SingleThreadedGCInBackgroundParallelPause,
             ("V8SingleThreadedGCInBackgroundParallelPause"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8SingleThreadedGCInBackgroundNoIncrementalMarking,
             "V8SingleThreadedGCInBackgroundNoIncrementalMarking",
             kFeatureDefaultStateControlledByV8);

// Use V8 efficiency mode for tiering decisions.
BASE_FEATURE(kV8EfficiencyModeTiering,
             "V8EfficiencyModeTiering",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<int> kV8EfficiencyModeTieringDelayTurbofan{
    &kV8EfficiencyModeTiering, "V8EfficiencyModeTieringDelayTurbofan", 15000};

// Enables slow histograms that provide detailed information at increased
// runtime overheads.
BASE_FEATURE(kV8SlowHistograms,
             ("V8SlowHistograms"),
             kFeatureDefaultStateControlledByV8);
// Multiple finch experiments might use slow-histograms. We introduce
// separate feature flags to circumvent finch limitations.
BASE_FEATURE(kV8SlowHistogramsCodeMemoryWriteProtection,
             "V8SlowHistogramsCodeMemoryWriteProtection",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsIntelJCCErratumMitigation,
             "V8SlowHistogramsIntelJCCErratumMitigation",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsSparkplug,
             "V8SlowHistogramsSparkplug",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsSparkplugAndroid,
             "V8SlowHistogramsSparkplugAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsNoTurbofan,
             "V8SlowHistogramsNoTurbofan",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enable 16GB heap reservation for Oilpan.
BASE_FEATURE(kV8CppGCEnableLargerCage,
             "V8CppGCEnableLargerCage",
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8DelayMemoryReducer,
             "V8DelayMemoryReducer",
             base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kV8MemoryReducerStartDelay{
    &kV8DelayMemoryReducer, "delay", base::Seconds(30)};

BASE_FEATURE(kV8ConcurrentMarkingHighPriorityThreads,
             ("V8ConcurrentMarkingHighPriorityThreads"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8UpdateLimitAfterLoading,
             ("V8UpdateLimitAfterLoading"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8UseLibmTrigFunctions,
             ("V8UseLibmTrigFunctions"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8UseOriginalMessageForStackTrace,
             ("V8UseOriginalMessageForStackTrace"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8IncrementalMarkingStartUserVisible,
             ("V8IncrementalMarkingStartUserVisible"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8IdleGcOnContextDisposal,
             ("V8IdleGcOnContextDisposal"),
             kFeatureDefaultStateControlledByV8);

// Elide redundant TDZ hole checks in bytecode. This only sets the V8 flag when
// manually overridden.
BASE_FEATURE(kV8IgnitionElideRedundantTdzChecks,
             ("V8IgnitionElideRedundantTdzChecks"),
             kFeatureDefaultStateControlledByV8);

// Add additional alignment for some jumps in generated x64 code, to mitigate
// the performance impact of the Intel JCC erratum (https://crbug.com/v8/14225).
// Currently disabled by default in V8, but adding here temporarily to test
// real-world performance impact via a Finch experiment.
BASE_FEATURE(kV8IntelJCCErratumMitigation,
             ("V8IntelJCCErratumMitigation"),
             kFeatureDefaultStateControlledByV8);

// JavaScript language features.

// Enables the experiment with compile hints as magic comments.
BASE_FEATURE(kJavaScriptCompileHintsMagic,
             ("JavaScriptCompileHintsMagic"),
             kFeatureDefaultStateControlledByV8);

// Enables the iterator helpers proposal.
BASE_FEATURE(kJavaScriptIteratorHelpers,
             ("kJavaScriptIteratorHelpers"),
             kFeatureDefaultStateControlledByV8);

// Enables the Promise.withResolvers proposal.
BASE_FEATURE(kJavaScriptPromiseWithResolvers,
             ("JavaScriptPromiseWithResolvers"),
             kFeatureDefaultStateControlledByV8);

// Enables the RegExp modifiers proposal.
BASE_FEATURE(kJavaScriptRegExpModifiers,
             ("JavaScriptRegExpModifiers"),
             kFeatureDefaultStateControlledByV8);

// Enables the `with` syntax for the Import Attributes proposal.
BASE_FEATURE(kJavaScriptImportAttributes,
             ("JavaScriptImportAttributes"),
             kFeatureDefaultStateControlledByV8);

// Enables the set methods proposal.
BASE_FEATURE(kJavaScriptSetMethods,
             ("JavaScriptSetMethods"),
             kFeatureDefaultStateControlledByV8);

// Enables the RegExp duplicate named capture groups proposal.
BASE_FEATURE(kJavaScriptRegExpDuplicateNamedGroups,
             ("JavaScriptRegExpDuplicateNamedGroups"),
             kFeatureDefaultStateControlledByV8);

// Enables the Promise.try proposal.
BASE_FEATURE(kJavaScriptPromiseTry,
             ("JavaScriptPromiseTry"),
             kFeatureDefaultStateControlledByV8);

// WebAssembly features.

// Enable WebAssembly deoptimization support (not user visible), see
// https://crbug.com/42204618.
BASE_FEATURE(kWebAssemblyDeopt,
             "WebAssemblyDeopt",
             kFeatureDefaultStateControlledByV8);

// Feature for WebAssembly speculative inlining of indirect calls (see
// https://crbug.com/335082212; and https://crbug.com/40898108 for direct call
// and call_ref inlining, which has already launched above). Not user visible.
BASE_FEATURE(kWebAssemblyInliningCallIndirect,
             "WebAssemblyInliningCallIndirect",
             kFeatureDefaultStateControlledByV8);

// Enable WebAssembly code flushing.
BASE_FEATURE(kWebAssemblyLiftoffCodeFlushing,
             ("WebAssemblyLiftoffCodeFlushing"),
             kFeatureDefaultStateControlledByV8);

// Enable support for multiple memories according to the multi-memory proposal:
// https://github.com/WebAssembly/multi-memory. See
// https://chromestatus.com/feature/5106389887746048.
BASE_FEATURE(kWebAssemblyMultipleMemories,
             ("WebAssemblyMultipleMemories"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kWebAssemblyTurboshaft,
             ("WebAssemblyTurboshaft"),
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kWebAssemblyTurboshaftInstructionSelection,
             ("WebAssemblyTurboshaftInstructionSelection"),
             kFeatureDefaultStateControlledByV8);

// Feature for more aggressive code caching (https://crbug.com/v8/14411,
// https://crbug.com/40945417) and three parameters to control caching behavior.
BASE_FEATURE(kWebAssemblyMoreAggressiveCodeCaching,
             "WebAssemblyMoreAggressiveCodeCaching",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<int> kWebAssemblyMoreAggressiveCodeCachingThreshold{
    &kWebAssemblyMoreAggressiveCodeCaching, "WebAssemblyCodeCachingThreshold",
    1'000};
const base::FeatureParam<int> kWebAssemblyMoreAggressiveCodeCachingTimeoutMs{
    &kWebAssemblyMoreAggressiveCodeCaching, "WebAssemblyCodeCachingTimeoutMs",
    2000};
const base::FeatureParam<int>
    kWebAssemblyMoreAggressiveCodeCachingHardThreshold{
        &kWebAssemblyMoreAggressiveCodeCaching,
        "WebAssemblyCodeCachingHardThreshold", 1'000'000};

}  // namespace features
