// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_SESSION_OPTIONS_H_
#define SERVICES_WEBNN_ORT_ORT_SESSION_OPTIONS_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "base/types/pass_key.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_context_provider.mojom.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "services/webnn/public/mojom/webnn_service_introspection.mojom-forward.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn {

struct EpDeviceInfo;

namespace ort {

class Environment;

// `SessionOptions` is a wrapper of `OrtSessionOptions` and used to create
// sessions on background threads.
class SessionOptions final : public base::RefCountedThreadSafe<SessionOptions> {
 public:
  // Applies the auto EP selection policy to configure the EPs based on
  // `context_options`.
  static base::expected<scoped_refptr<SessionOptions>, std::string> Create(
      mojom::CreateContextOptionsPtr context_options,
      scoped_refptr<Environment> env);

  // Selects the target EP device directly, bypassing the auto EP selection
  // policy.
  static scoped_refptr<SessionOptions> Create(const EpDeviceInfo& target_device,
                                              scoped_refptr<Environment> env);

  SessionOptions(base::PassKey<SessionOptions>,
                 ScopedOrtSessionOptions session_options,
                 scoped_refptr<Environment> env,
                 const OrtEpDevice* first_selected_device,
                 mojom::CreateContextOptionsPtr context_options);

  SessionOptions(const SessionOptions&) = delete;
  SessionOptions& operator=(const SessionOptions&) = delete;

  const OrtSessionOptions* get() const { return session_options_.get(); }

  // Returns the first selected EP device for WebNN.
  const OrtEpDevice* first_selected_device() const {
    return first_selected_device_;
  }

  std::optional<uint32_t> batched_matmul_k_dimension_limit() const {
    return batched_matmul_k_dimension_limit_;
  }

 private:
  friend class base::RefCountedThreadSafe<SessionOptions>;

  ~SessionOptions();

  ScopedOrtSessionOptions session_options_;
  scoped_refptr<Environment> env_;
  // It's safe to keep `first_selected_device_` as `env_` owns all EP devices.
  raw_ptr<const OrtEpDevice> first_selected_device_;

  std::optional<uint32_t> batched_matmul_k_dimension_limit_;

  // EP selection policy delegate selects EPs based on the context options.
  // Nullptr if the target EP device is specified directly.
  const mojom::CreateContextOptionsPtr context_options_;
};

}  // namespace ort

}  // namespace webnn

#endif  // SERVICES_WEBNN_ORT_ORT_SESSION_OPTIONS_H_
