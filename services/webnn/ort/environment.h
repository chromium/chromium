// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ENVIRONMENT_H_
#define SERVICES_WEBNN_ORT_ENVIRONMENT_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/synchronization/lock.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "gpu/config/gpu_info.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

// Describes the workarounds needed for execution provider limitations.
// TODO(crbug.com/428740146): Remove this struct once all the execution
// providers fix these issues.
struct EpWorkarounds {
  // TODO(crbug.com/429253567): Specify the minimum package version that
  // supports these features without requiring workarounds.

  bool disable_external_data = false;
  // By default ONNX Resize op supports any axes, but some EPs may only support
  // NCHW layout. `ContextProperties.resample_2d_axes` setting will respect to
  // this limit.
  bool resample2d_limit_to_nchw = false;

  EpWorkarounds& operator|=(const EpWorkarounds& other) {
    disable_external_data |= other.disable_external_data;
    resample2d_limit_to_nchw |= other.resample2d_limit_to_nchw;
    return *this;
  }
};

// A wrapper of `OrtEnv` which is thread-safe and can be shared across sessions.
// It should be kept alive until all sessions using it are destroyed.
class Environment : public base::subtle::RefCountedThreadSafeBase {
 public:
  REQUIRE_ADOPTION_FOR_REFCOUNTED_TYPE();

  static base::expected<scoped_refptr<Environment>, std::string> GetInstance(
      const gpu::GPUInfo& gpu_info);

  Environment(base::PassKey<Environment> pass_key, ScopedOrtEnv env);
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  void AddRef() const;
  void Release() const;

  EpWorkarounds GetEpWorkarounds(mojom::Device device_type) const;

  const OrtEnv* get() const { return env_.get(); }

 private:
  static base::expected<scoped_refptr<Environment>, std::string> Create(
      const gpu::GPUInfo& gpu_info);

  ~Environment();

  ScopedOrtEnv env_;

  static base::Lock& GetLock();
  // Make `Environment` a singleton to avoid duplicate `OrtEnv` creation.
  static raw_ptr<Environment> instance_ GUARDED_BY(GetLock());
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ENVIRONMENT_H_
