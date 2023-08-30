// Copyright (c) 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_ISOLATION_KEY_PROVIDER_H_
#define GPU_COMMAND_BUFFER_SERVICE_ISOLATION_KEY_PROVIDER_H_

#include "base/functional/callback.h"
#include "gpu/gpu_gles2_export.h"
#include "third_party/blink/public/common/tokens/tokens.h"

namespace gpu {

// Interface that can be implemented and passed to users (i.e. decoders) to get
// an isolation key given an token. Note that in most cases, the token is
// passed via the client so it cannot necessarily be trusted. As a result, this
// provider is a layer of security that may either do validation on the token,
// or ask a trusted source for an isolation key given the client and frame. This
// way, the potential vulnerability surface is limited to at most the
// untrusted client.
// TODO(dawn:549) The interface is taking a WebGPUExecutionContextToken since it
//     is only used in WebGPU, and DocumentToken is not included in the default
//     ExecutionContextToken yet. This interface may be updated to use the
//     ExecutionContextToken should DocumentToken become a part of it.
class GPU_GLES2_EXPORT IsolationKeyProvider {
 public:
  using GetIsolationKeyCallback =
      base::OnceCallback<void(const std::string& isolation_key)>;

  virtual void GetIsolationKey(const blink::WebGPUExecutionContextToken& token,
                               GetIsolationKeyCallback cb) = 0;

 protected:
  virtual ~IsolationKeyProvider() = default;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_ISOLATION_KEY_PROVIDER_H_
