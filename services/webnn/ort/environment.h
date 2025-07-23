// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ENVIRONMENT_H_
#define SERVICES_WEBNN_ORT_ENVIRONMENT_H_

#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "gpu/config/gpu_info.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "third_party/onnxruntime_headers/src/include/onnxruntime/core/session/onnxruntime_c_api.h"

namespace webnn::ort {

// A wrapper of `OrtEnv` which is thread-safe and can be shared across sessions.
// It should be kept alive until all sessions using it are destroyed.
class Environment : public base::RefCountedThreadSafe<Environment> {
 public:
  static base::expected<scoped_refptr<Environment>, std::string> Create(
      const gpu::GPUInfo& gpu_info);

  Environment(base::PassKey<Environment> pass_key, ScopedOrtEnv env);
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;

  const OrtEnv* get() const { return env_.get(); }

  bool IsExternalDataSupported(mojom::Device device_type) const;

 private:
  friend class base::RefCountedThreadSafe<Environment>;

  ~Environment();

  ScopedOrtEnv env_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ENVIRONMENT_H_
