// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/ipc/common/constants_mojom_traits.h"

#include "base/notreached.h"

namespace mojo {

// static
gpu::mojom::Error EnumTraits<gpu::mojom::Error, gpu::error::Error>::ToMojom(
    gpu::error::Error error) {
  switch (error) {
    case gpu::error::kNoError:
      return gpu::mojom::Error::kNoError;
    case gpu::error::kInvalidSize:
      return gpu::mojom::Error::kInvalidSize;
    case gpu::error::kOutOfBounds:
      return gpu::mojom::Error::kOutOfBounds;
    case gpu::error::kUnknownCommand:
      return gpu::mojom::Error::kUnknownCommand;
    case gpu::error::kInvalidArguments:
      return gpu::mojom::Error::kInvalidArguments;
    case gpu::error::kLostContext:
      return gpu::mojom::Error::kLostContext;
    case gpu::error::kGenericError:
      return gpu::mojom::Error::kGenericError;
    case gpu::error::kDeferCommandUntilLater:
      return gpu::mojom::Error::kDeferCommandUntilLater;
    case gpu::error::kDeferLaterCommands:
      return gpu::mojom::Error::kDeferLaterCommands;
  }
  NOTREACHED();
}

// static
bool EnumTraits<gpu::mojom::Error, gpu::error::Error>::FromMojom(
    gpu::mojom::Error input,
    gpu::error::Error* out) {
  switch (input) {
    case gpu::mojom::Error::kNoError:
      *out = gpu::error::kNoError;
      return true;
    case gpu::mojom::Error::kInvalidSize:
      *out = gpu::error::kInvalidSize;
      return true;
    case gpu::mojom::Error::kOutOfBounds:
      *out = gpu::error::kOutOfBounds;
      return true;
    case gpu::mojom::Error::kUnknownCommand:
      *out = gpu::error::kUnknownCommand;
      return true;
    case gpu::mojom::Error::kInvalidArguments:
      *out = gpu::error::kInvalidArguments;
      return true;
    case gpu::mojom::Error::kLostContext:
      *out = gpu::error::kLostContext;
      return true;
    case gpu::mojom::Error::kGenericError:
      *out = gpu::error::kGenericError;
      return true;
    case gpu::mojom::Error::kDeferCommandUntilLater:
      *out = gpu::error::kDeferCommandUntilLater;
      return true;
    case gpu::mojom::Error::kDeferLaterCommands:
      *out = gpu::error::kDeferLaterCommands;
      return true;
  }
  return false;
}

// static
gpu::mojom::ContextLostReason
EnumTraits<gpu::mojom::ContextLostReason, gpu::error::ContextLostReason>::
    ToMojom(gpu::error::ContextLostReason reason) {
  switch (reason) {
    case gpu::error::kGuilty:
      return gpu::mojom::ContextLostReason::kGuilty;
    case gpu::error::kInnocent:
      return gpu::mojom::ContextLostReason::kInnocent;
    case gpu::error::kUnknown:
      return gpu::mojom::ContextLostReason::kUnknown;
    case gpu::error::kOutOfMemory:
      return gpu::mojom::ContextLostReason::kOutOfMemory;
    case gpu::error::kMakeCurrentFailed:
      return gpu::mojom::ContextLostReason::kMakeCurrentFailed;
    case gpu::error::kGpuChannelLost:
      return gpu::mojom::ContextLostReason::kGpuChannelLost;
    case gpu::error::kInvalidGpuMessage:
      return gpu::mojom::ContextLostReason::kInvalidGpuMessage;
  }
  NOTREACHED();
}

// static
bool EnumTraits<gpu::mojom::ContextLostReason, gpu::error::ContextLostReason>::
    FromMojom(gpu::mojom::ContextLostReason input,
              gpu::error::ContextLostReason* out) {
  switch (input) {
    case gpu::mojom::ContextLostReason::kGuilty:
      *out = gpu::error::kGuilty;
      return true;
    case gpu::mojom::ContextLostReason::kInnocent:
      *out = gpu::error::kInnocent;
      return true;
    case gpu::mojom::ContextLostReason::kUnknown:
      *out = gpu::error::kUnknown;
      return true;
    case gpu::mojom::ContextLostReason::kOutOfMemory:
      *out = gpu::error::kOutOfMemory;
      return true;
    case gpu::mojom::ContextLostReason::kMakeCurrentFailed:
      *out = gpu::error::kMakeCurrentFailed;
      return true;
    case gpu::mojom::ContextLostReason::kGpuChannelLost:
      *out = gpu::error::kGpuChannelLost;
      return true;
    case gpu::mojom::ContextLostReason::kInvalidGpuMessage:
      *out = gpu::error::kInvalidGpuMessage;
      return true;
  }
  return false;
}

}  // namespace mojo
