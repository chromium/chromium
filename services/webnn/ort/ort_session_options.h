// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_ORT_ORT_SESSION_OPTIONS_H_
#define SERVICES_WEBNN_ORT_ORT_SESSION_OPTIONS_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/types/pass_key.h"
#include "services/webnn/ort/scoped_ort_types.h"
#include "services/webnn/public/mojom/webnn_device.mojom.h"
#include "services/webnn/public/mojom/webnn_error.mojom.h"
#include "third_party/windows_app_sdk_headers/src/inc/abi/winml/winml/onnxruntime_c_api.h"

namespace webnn::ort {

class Environment;

// `SessionOptions` is a wrapper of `OrtSessionOptions` and used to create
// sessions on background threads.
class SessionOptions final : public base::RefCountedThreadSafe<SessionOptions> {
 public:
  // The `device_type` would be used to configure ONNX Runtime EP.
  static scoped_refptr<SessionOptions> Create(mojom::Device device_type,
                                              scoped_refptr<Environment> env);

  SessionOptions(base::PassKey<SessionOptions>,
                 ScopedOrtSessionOptions session_options,
                 mojom::Device device_type);

  SessionOptions(const SessionOptions&) = delete;
  SessionOptions& operator=(const SessionOptions&) = delete;

  const OrtSessionOptions* get() const { return session_options_.get(); }

 private:
  friend class base::RefCountedThreadSafe<SessionOptions>;

  ~SessionOptions();

  ScopedOrtSessionOptions session_options_;
  const mojom::Device device_type_;
};

}  // namespace webnn::ort

#endif  // SERVICES_WEBNN_ORT_ORT_SESSION_OPTIONS_H_
