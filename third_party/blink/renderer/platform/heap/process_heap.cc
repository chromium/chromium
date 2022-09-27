// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/heap/process_heap.h"
#include "gin/public/cppgc.h"

namespace blink {

// static
void ProcessHeap::Init() {
  gin::InitializeCppgcFromV8Platform();
}

}  // namespace blink
