// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_GIN_FEATURES_H_
#define GIN_GIN_FEATURES_H_

#include "base/feature_list.h"
#include "gin/gin_export.h"

namespace features {

GIN_EXPORT extern const base::Feature kV8OptimizeJavascript;
GIN_EXPORT extern const base::Feature kV8FlushBytecode;
GIN_EXPORT extern const base::Feature kV8LazyFeedbackAllocation;
GIN_EXPORT extern const base::Feature kV8MemoryReducerForSmallHeaps;
GIN_EXPORT extern const base::Feature kV8HugeMaxOldGenerationSize;
GIN_EXPORT extern const base::Feature kV8GCBackgroundSchedule;
GIN_EXPORT extern const base::Feature kV8GCLessCompaction;
GIN_EXPORT extern const base::Feature kV8GCAlwaysPromoteYoungMC;

}  // namespace features

#endif  // GIN_GIN_FEATURES_H_
