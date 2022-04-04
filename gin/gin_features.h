// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_GIN_FEATURES_H_
#define GIN_GIN_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "gin/gin_export.h"

namespace features {

GIN_EXPORT extern const base::Feature kV8CompactCodeSpaceWithStack;
GIN_EXPORT extern const base::Feature kV8CompactMaps;
GIN_EXPORT extern const base::Feature kV8CompactWithStack;
GIN_EXPORT extern const base::Feature kV8ConcurrentSparkplug;
GIN_EXPORT extern const base::FeatureParam<int>
    kV8ConcurrentSparkplugMaxThreads;
GIN_EXPORT extern const base::Feature kV8CrashOnEvacuationFailure;
GIN_EXPORT extern const base::Feature kV8ExperimentalRegexpEngine;
GIN_EXPORT extern const base::Feature kV8FlushBytecode;
GIN_EXPORT extern const base::Feature kV8FlushBaselineCode;
GIN_EXPORT extern const base::Feature kV8FlushEmbeddedBlobICache;
GIN_EXPORT extern const base::Feature kV8LazyFeedbackAllocation;
GIN_EXPORT extern const base::Feature kV8NoReclaimUnmodifiedWrappers;
GIN_EXPORT extern const base::Feature kV8CodeMemoryWriteProtection;
GIN_EXPORT extern const base::Feature kV8OffThreadFinalization;
GIN_EXPORT extern const base::Feature kV8OptimizeJavascript;
GIN_EXPORT extern const base::Feature kV8PerContextMarkingWorklist;
GIN_EXPORT extern const base::Feature kV8ReduceConcurrentMarkingTasks;
GIN_EXPORT extern const base::Feature kV8ScriptAblation;
GIN_EXPORT extern const base::FeatureParam<double> kV8ScriptDelayFraction;
GIN_EXPORT extern const base::FeatureParam<int> kV8ScriptDelayMs;
GIN_EXPORT extern const base::FeatureParam<int> kV8ScriptDelayOnceMs;
GIN_EXPORT extern const base::Feature kV8ShortBuiltinCalls;
GIN_EXPORT extern const base::Feature kV8SlowHistograms;
GIN_EXPORT extern const base::Feature
    kV8SlowHistogramsCodeMemoryWriteProtection;
GIN_EXPORT extern const base::Feature kV8SlowHistogramsSparkplug;
GIN_EXPORT extern const base::Feature kV8SlowHistogramsSparkplugAndroid;
GIN_EXPORT extern const base::Feature kV8SlowHistogramsScriptAblation;
GIN_EXPORT extern const base::Feature kV8Sparkplug;
GIN_EXPORT extern const base::Feature kV8SparkplugNeedsShortBuiltinCalls;
GIN_EXPORT extern const base::Feature kV8TurboFastApiCalls;
GIN_EXPORT extern const base::Feature kV8Turboprop;
GIN_EXPORT extern const base::Feature kV8UseMapSpace;
GIN_EXPORT extern const base::Feature kV8VirtualMemoryCage;

}  // namespace features

#endif  // GIN_GIN_FEATURES_H_
