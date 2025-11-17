// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/gin_features.h"

#include "base/metrics/field_trial_params.h"

namespace features {

// Enable code space compaction when finalizing a full GC with stack.
BASE_FEATURE(kV8CompactCodeSpaceWithStack, kFeatureDefaultStateControlledByV8);

// Enable compaction when finalizing a full GC with stack.
BASE_FEATURE(kV8CompactWithStack, kFeatureDefaultStateControlledByV8);

// Decommit (rather than discard) pooled pages.
BASE_FEATURE(kV8DecommitPooledPages,
             "DecommitPooledPages",
             kFeatureDefaultStateControlledByV8);

// Enables optimization of JavaScript in V8.
BASE_FEATURE(kV8OptimizeJavascript, kFeatureDefaultStateControlledByV8);

// Enables flushing of JS bytecode in V8.
BASE_FEATURE(kV8FlushBytecode, kFeatureDefaultStateControlledByV8);
const base::FeatureParam<int> kV8FlushBytecodeOldAge{
    &kV8FlushBytecode, "V8FlushBytecodeOldAge", 5};

// Enables flushing of baseline code in V8.
BASE_FEATURE(kV8FlushBaselineCode, kFeatureDefaultStateControlledByV8);

// Enables code flushing based on tab visibility.
BASE_FEATURE(kV8FlushCodeBasedOnTabVisibility,
             kFeatureDefaultStateControlledByV8);

// Enables code flushing based on time.
BASE_FEATURE(kV8FlushCodeBasedOnTime, kFeatureDefaultStateControlledByV8);
const base::FeatureParam<int> kV8FlushCodeOldTime{&kV8FlushCodeBasedOnTime,
                                                  "V8FlushCodeOldTime", 30};

// Enables finalizing streaming JS compilations on a background thread.
BASE_FEATURE(kV8OffThreadFinalization, kFeatureDefaultStateControlledByV8);

// Enables lazy feedback allocation in V8.
BASE_FEATURE(kV8LazyFeedbackAllocation, kFeatureDefaultStateControlledByV8);

// Enables per-context marking worklists in V8 GC.
BASE_FEATURE(kV8PerContextMarkingWorklist, kFeatureDefaultStateControlledByV8);

// Enables flushing of the instruction cache for the embedded blob.
BASE_FEATURE(kV8FlushEmbeddedBlobICache, kFeatureDefaultStateControlledByV8);

// Enables reduced number of concurrent marking tasks.
BASE_FEATURE(kV8ReduceConcurrentMarkingTasks,
             kFeatureDefaultStateControlledByV8);

// Disables reclaiming of unmodified wrappers objects.
BASE_FEATURE(kV8NoReclaimUnmodifiedWrappers,
             kFeatureDefaultStateControlledByV8);

// Enables W^X code memory protection in V8.
// This is enabled in V8 by default. To test the performance impact, we are
// going to disable this feature in a finch experiment.
BASE_FEATURE(kV8CodeMemoryWriteProtection, kFeatureDefaultStateControlledByV8);

// Enables fallback to a breadth-first regexp engine on excessive backtracking.
BASE_FEATURE(kV8ExperimentalRegexpEngine, kFeatureDefaultStateControlledByV8);

// Enable accounting for external memory limits as part of global limits in v8
// Heap.
BASE_FEATURE(kV8ExternalMemoryAccountedInGlobalLimit,
             kFeatureDefaultStateControlledByV8);

// Enables the Turbofan compiler.
BASE_FEATURE(kV8Turbofan, kFeatureDefaultStateControlledByV8);

// Enables Turbofan's new compiler IR Turboshaft.
BASE_FEATURE(kV8Turboshaft, kFeatureDefaultStateControlledByV8);

// Enable running instruction selection on Turboshaft IR directly.
BASE_FEATURE(kV8TurboshaftInstructionSelection,
             kFeatureDefaultStateControlledByV8);

// Enables Maglev compiler. Note that this only sets the V8 flag when
// manually overridden; otherwise it defers to whatever the V8 default is.
BASE_FEATURE(kV8Maglev, kFeatureDefaultStateControlledByV8);
BASE_FEATURE(kV8ConcurrentMaglevHighPriorityThreads,
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8HighEndAndroid, kFeatureDefaultStateControlledByV8);

const base::FeatureParam<int> kV8HighEndAndroidMemoryThreshold{
    &kV8HighEndAndroid, "V8HighEndAndroidMemoryThreshold", 8};

BASE_FEATURE(kV8MemoryReducer, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kV8MemoryReducerGCCount{
    &kV8MemoryReducer, "V8MemoryReducerGCCount", 3};

BASE_FEATURE(kV8MemoryPoolReleaseOnMallocFailures,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kV8PreconfigureOldGen, kFeatureDefaultStateControlledByV8);

const base::FeatureParam<int> kV8PreconfigureOldGenSize{
    &kV8PreconfigureOldGen, "V8PreconfigureOldGenSize", 32};

// Enables MinorMC young generation garbage collector.
BASE_FEATURE(kV8MinorMS, kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8ScavengerHigherCapacity, base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kV8ScavengerMaxCapacity{
    &kV8ScavengerHigherCapacity, "V8ScavengerMaxCapacity", 16};

// Enables Sparkplug compiler. Note that this only sets the V8 flag when
// manually overridden; otherwise it defers to whatever the V8 default is.
BASE_FEATURE(kV8Sparkplug, kFeatureDefaultStateControlledByV8);

// Enables the concurrent Sparkplug compiler.
BASE_FEATURE(kV8ConcurrentSparkplug, kFeatureDefaultStateControlledByV8);
const base::FeatureParam<int> kV8ConcurrentSparkplugMaxThreads{
    &kV8ConcurrentSparkplug, "V8ConcurrentSparkplugMaxThreads", 0};
BASE_FEATURE(kV8ConcurrentSparkplugHighPriorityThreads,
             kFeatureDefaultStateControlledByV8);
// Makes sure the experimental Sparkplug compiler is only enabled if short
// builtin calls are enabled too.
BASE_FEATURE(kV8SparkplugNeedsShortBuiltinCalls,
             kFeatureDefaultStateControlledByV8);

// Enables batch compilation for Sparkplug (baseline) compilation.
BASE_FEATURE(kV8BaselineBatchCompilation, kFeatureDefaultStateControlledByV8);

// Enables short builtin calls feature.
BASE_FEATURE(kV8ShortBuiltinCalls, kFeatureDefaultStateControlledByV8);

// Enables fast API calls in TurboFan.
BASE_FEATURE(kV8TurboFastApiCalls, kFeatureDefaultStateControlledByV8);

// Enables faster DOM methods for megamorphic ICs
BASE_FEATURE(kV8MegaDomIC, kFeatureDefaultStateControlledByV8);

// Faster object cloning
BASE_FEATURE(kV8SideStepTransitions, kFeatureDefaultStateControlledByV8);

// Avoids background threads for GC if isolate is in background.
BASE_FEATURE(kV8SingleThreadedGCInBackground,
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8SingleThreadedGCInBackgroundParallelPause,
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8SingleThreadedGCInBackgroundNoIncrementalMarking,
             kFeatureDefaultStateControlledByV8);

// Enables slow histograms that provide detailed information at increased
// runtime overheads.
BASE_FEATURE(kV8SlowHistograms, kFeatureDefaultStateControlledByV8);
// Multiple finch experiments might use slow-histograms. We introduce
// separate feature flags to circumvent finch limitations.
BASE_FEATURE(kV8SlowHistogramsCodeMemoryWriteProtection,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsSparkplug, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsSparkplugAndroid,
             base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kV8SlowHistogramsNoTurbofan, base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kV8DelayMemoryReducer, base::FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kV8MemoryReducerStartDelay{
    &kV8DelayMemoryReducer, "delay", base::Seconds(30)};

BASE_FEATURE(kV8ConcurrentMarkingHighPriorityThreads,
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8UseLibmTrigFunctions, kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8UseOriginalMessageForStackTrace,
             kFeatureDefaultStateControlledByV8);

BASE_FEATURE(kV8IdleGcOnContextDisposal, kFeatureDefaultStateControlledByV8);

// Elide redundant TDZ hole checks in bytecode. This only sets the V8 flag when
// manually overridden.
BASE_FEATURE(kV8IgnitionElideRedundantTdzChecks,
             kFeatureDefaultStateControlledByV8);

// JavaScript language features.

// Enables the RegExp modifiers proposal.
BASE_FEATURE(kJavaScriptRegExpModifiers, kFeatureDefaultStateControlledByV8);

// Enables the `with` syntax for the Import Attributes proposal.
BASE_FEATURE(kJavaScriptImportAttributes, kFeatureDefaultStateControlledByV8);

// Enables the RegExp duplicate named capture groups proposal.
BASE_FEATURE(kJavaScriptRegExpDuplicateNamedGroups,
             kFeatureDefaultStateControlledByV8);

// Enables the Promise.try proposal.
BASE_FEATURE(kJavaScriptPromiseTry, kFeatureDefaultStateControlledByV8);

// WebAssembly features (currently none).

}  // namespace features
