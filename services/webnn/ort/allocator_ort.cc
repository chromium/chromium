// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/webnn/ort/allocator_ort.h"

#include "services/webnn/ort/error_ort.h"
#include "services/webnn/ort/platform_functions_ort.h"
#include "services/webnn/ort/utils_ort.h"

namespace webnn::ort {

// static
scoped_refptr<AllocatorOrt> AllocatorOrt::GetInstance() {
  // If the `AllocatorOrt` instance is created, add a reference and return it.
  if (instance_) {
    return base::WrapRefCounted(instance_);
  }

  return AllocatorOrt::Create();
}

// static
scoped_refptr<AllocatorOrt> AllocatorOrt::Create() {
  const OrtApi* ort_api = GetOrtApi();
  OrtEnv* env;
  CHECK_STATUS(ort_api->CreateEnv(ORT_LOGGING_LEVEL_WARNING, "test", &env));
  CHECK(env);

  OrtMemoryInfo* memory_info;
  CHECK_STATUS(ort_api->CreateCpuMemoryInfo(OrtDeviceAllocator,
                                            OrtMemTypeDefault, &memory_info));
  CHECK(memory_info);

  OrtAllocator* allocator = nullptr;
  // The default allocator is a CPU based, non-arena. Always returns the same
  // pointer to the same default allocator. Returned value should NOT be freed.
  CHECK_STATUS(ort_api->GetAllocatorWithDefaultOptions(&allocator));
  CHECK(allocator);
  return WrapRefCounted(new AllocatorOrt(env, memory_info, allocator));
}

AllocatorOrt::AllocatorOrt(OrtEnv* env,
                           OrtMemoryInfo* memory_info,
                           OrtAllocator* allocator)
    : env_(env), memory_info_(memory_info), allocator_(allocator) {
  instance_ = this;
}

AllocatorOrt::~AllocatorOrt() {
  instance_ = nullptr;

  const OrtApi* ort_api = GetOrtApi();
  ort_api->ReleaseMemoryInfo(memory_info_);
  ort_api->ReleaseEnv(env_);
}

AllocatorOrt* AllocatorOrt::instance_ = nullptr;

}  // namespace webnn::ort
