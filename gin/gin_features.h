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
GIN_EXPORT extern const base::Feature kV8OffThreadFinalization;
GIN_EXPORT extern const base::Feature kV8LazyFeedbackAllocation;
GIN_EXPORT extern const base::Feature kV8ConcurrentInlining;
GIN_EXPORT extern const base::Feature kV8PerContextMarkingWorklist;
GIN_EXPORT extern const base::Feature kV8FlushEmbeddedBlobICache;
GIN_EXPORT extern const base::Feature kV8ReduceConcurrentMarkingTasks;
GIN_EXPORT extern const base::Feature kV8NoReclaimUnmodifiedWrappers;
GIN_EXPORT extern const base::Feature kV8LocalHeaps;
GIN_EXPORT extern const base::Feature kV8TurboDirectHeapAccess;

}  // namespace features

#endif  // GIN_GIN_FEATURES_H_
