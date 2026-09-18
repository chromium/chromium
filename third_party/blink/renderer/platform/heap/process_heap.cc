// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/process_heap.h"

#include "base/feature_list.h"
#include "gin/public/cppgc.h"
#include "third_party/blink/public/common/features.h"

namespace blink {

bool ProcessHeap::is_heap_vector_promptly_free_enabled_ = true;

// static
void ProcessHeap::Init() {
  is_heap_vector_promptly_free_enabled_ =
      base::FeatureList::IsEnabled(features::kHeapVectorPromptlyFree);
  gin::InitializeCppgcFromV8Platform();
}

}  // namespace blink
