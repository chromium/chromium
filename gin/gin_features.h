// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_GIN_FEATURES_H_
#define GIN_GIN_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "gin/gin_export.h"

namespace features {

// For many V8 flags, the canonical location of the default value of the flag is
// inside V8. For these flags, the equivalent Chromium base::Feature state is
// ignored if it is not explicitly overridden. For these cases, the
// base::Feature default value is arbitrary and this constant is used for
// documentation.
inline constexpr base::FeatureState kFeatureDefaultStateControlledByV8 =
    base::FEATURE_DISABLED_BY_DEFAULT;

GIN_EXPORT BASE_DECLARE_FEATURE(kV8CompactCodeSpaceWithStack);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8CompactWithStack);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ConcurrentSparkplug);
GIN_EXPORT extern const base::FeatureParam<int>
    kV8ConcurrentSparkplugMaxThreads;
GIN_EXPORT BASE_DECLARE_FEATURE(kV8BaselineBatchCompilation);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8CodeMemoryWriteProtection);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ConcurrentSparkplugHighPriorityThreads);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8CppGCEnableLargerCage);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8DelayMemoryReducer);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ConcurrentMarkingHighPriorityThreads);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8DecommitPooledPages);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ExperimentalRegexpEngine);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ExternalMemoryAccountedInGlobalLimit);
GIN_EXPORT extern const base::FeatureParam<int> kV8FlushBytecodeOldAge;
GIN_EXPORT BASE_DECLARE_FEATURE(kV8FlushBaselineCode);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8FlushBytecode);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8FlushCodeBasedOnTabVisibility);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8FlushCodeBasedOnTime);
GIN_EXPORT extern const base::FeatureParam<int> kV8FlushCodeOldTime;
GIN_EXPORT BASE_DECLARE_FEATURE(kV8FlushEmbeddedBlobICache);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8IncrementalMarkingStartUserVisible);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8IdleGcOnContextDisposal);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8IgnitionElideRedundantTdzChecks);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8IntelJCCErratumMitigation);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8LazyFeedbackAllocation);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8Maglev);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ConcurrentMaglevHighPriorityThreads);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8MemoryReducer);
GIN_EXPORT extern const base::FeatureParam<int> kV8MemoryReducerGCCount;
GIN_EXPORT BASE_DECLARE_FEATURE(kV8MinorMS);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8MegaDomIC);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8NoReclaimUnmodifiedWrappers);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8OffThreadFinalization);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8OptimizeJavascript);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8PerContextMarkingWorklist);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ReduceConcurrentMarkingTasks);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ScavengerHigherCapacity);
GIN_EXPORT extern const base::FeatureParam<int> kV8ScavengerMaxCapacity;
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SeparateGCPhases);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8ShortBuiltinCalls);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SideStepTransitions);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SingleThreadedGCInBackground);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SingleThreadedGCInBackgroundParallelPause);
GIN_EXPORT BASE_DECLARE_FEATURE(
    kV8SingleThreadedGCInBackgroundNoIncrementalMarking);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8EfficiencyModeTiering);
GIN_EXPORT extern const base::FeatureParam<int>
    kV8EfficiencyModeTieringDelayTurbofan;
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SlowHistograms);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SlowHistogramsCodeMemoryWriteProtection);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SlowHistogramsIntelJCCErratumMitigation);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SlowHistogramsNoTurbofan);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SlowHistogramsSparkplug);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SlowHistogramsSparkplugAndroid);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8Sparkplug);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8SparkplugNeedsShortBuiltinCalls);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8Turbofan);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8Turboshaft);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8TurboshaftInstructionSelection);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8TurboFastApiCalls);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8UpdateLimitAfterLoading);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8UseLibmTrigFunctions);
GIN_EXPORT BASE_DECLARE_FEATURE(kV8UseOriginalMessageForStackTrace);
GIN_EXPORT extern const base::FeatureParam<base::TimeDelta>
    kV8MemoryReducerStartDelay;
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptCompileHintsMagic);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptIteratorHelpers);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptPromiseWithResolvers);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptRegExpModifiers);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptImportAttributes);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptSetMethods);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptRegExpDuplicateNamedGroups);
GIN_EXPORT BASE_DECLARE_FEATURE(kJavaScriptPromiseTry);
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyDeopt);
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyInliningCallIndirect);
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyLiftoffCodeFlushing);
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyMultipleMemories);
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTurboshaft);
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyTurboshaftInstructionSelection);

// Feature for more aggressive code caching (https://crbug.com/v8/14411) and
// three parameters to control caching behavior.
GIN_EXPORT BASE_DECLARE_FEATURE(kWebAssemblyMoreAggressiveCodeCaching);
GIN_EXPORT extern const base::FeatureParam<int>
    kWebAssemblyMoreAggressiveCodeCachingThreshold;
GIN_EXPORT extern const base::FeatureParam<int>
    kWebAssemblyMoreAggressiveCodeCachingTimeoutMs;
GIN_EXPORT extern const base::FeatureParam<int>
    kWebAssemblyMoreAggressiveCodeCachingHardThreshold;

}  // namespace features

#endif  // GIN_GIN_FEATURES_H_
