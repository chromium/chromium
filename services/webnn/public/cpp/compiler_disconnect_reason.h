// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_WEBNN_PUBLIC_CPP_COMPILER_DISCONNECT_REASON_H_
#define SERVICES_WEBNN_PUBLIC_CPP_COMPILER_DISCONNECT_REASON_H_

#include <cstdint>

namespace webnn {

// Disconnect reasons passed via `mojo::Receiver::ResetWithReason()` when
// the WebNNCompilerService pipe is closed. Used by the browser process
// (`GpuProcessHost`) to distinguish intentional shutdown from unexpected
// crashes.
enum class CompilerDisconnectReason : uint32_t {
  // The compiler process shut down gracefully after all compiler contexts
  // disconnected and the idle timeout elapsed.
  kIdleShutdown = 1,
};

}  // namespace webnn

#endif  // SERVICES_WEBNN_PUBLIC_CPP_COMPILER_DISCONNECT_REASON_H_
