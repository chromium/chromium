// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/gin_features.h"

namespace features {

// Enables optimization of JavaScript in V8.
const base::Feature kV8OptimizeJavascript{"V8OptimizeJavascript",
                                          base::FEATURE_ENABLED_BY_DEFAULT};

// Enables flushing of JS bytecode in V8.
const base::Feature kV8FlushBytecode{"V8FlushBytecode",
                                     base::FEATURE_ENABLED_BY_DEFAULT};

// Enables lazy feedback allocation in V8.
const base::Feature kV8LazyFeedbackAllocation{"V8LazyFeedbackAllocation",
                                              base::FEATURE_ENABLED_BY_DEFAULT};

// Enables memory reducer for small heaps in V8.
const base::Feature kV8MemoryReducerForSmallHeaps{
    "V8MemoryReducerForSmallHeaps", base::FEATURE_ENABLED_BY_DEFAULT};

// Increase V8 heap size to 4GB if the physical memory is bigger than 16 GB.
const base::Feature kV8HugeMaxOldGenerationSize{
    "V8HugeMaxOldGenerationSize", base::FEATURE_ENABLED_BY_DEFAULT};

// Enables new background GC scheduling heuristics.
const base::Feature kV8GCBackgroundSchedule{"V8GCBackgroundSchedule",
                                            base::FEATURE_DISABLED_BY_DEFAULT};

// Perform less compaction in non-memory reducing mode.
const base::Feature kV8GCLessCompaction{"V8GCLessCompaction",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

// Always promote young objects in Mark-Compact GC.
const base::Feature kV8GCAlwaysPromoteYoungMC{
    "V8GCAlwaysPromoteYoungMC", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
