// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/graphics/gpu/webgpu_native_test_support.h"

#include <dawn/native/DawnNative.h>

namespace blink {

const DawnProcTable& GetDawnNativeProcs() {
  return dawn::native::GetProcs();
}
WGPUInstance MakeNativeWGPUInstance() {
  auto instance = std::make_unique<dawn::native::Instance>();
  dawn::native::GetProcs().instanceAddRef(instance->Get());
  return instance->Get();
}

}  // namespace blink
