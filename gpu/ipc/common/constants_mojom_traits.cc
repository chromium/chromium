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

// static
gpu::mojom::CommandBufferNamespace
EnumTraits<gpu::mojom::CommandBufferNamespace, gpu::CommandBufferNamespace>::
    ToMojom(gpu::CommandBufferNamespace namespace_id) {
  switch (namespace_id) {
    case gpu::CommandBufferNamespace::INVALID:
      return gpu::mojom::CommandBufferNamespace::INVALID;
    case gpu::CommandBufferNamespace::GPU_IO:
      return gpu::mojom::CommandBufferNamespace::GPU_IO;
    case gpu::CommandBufferNamespace::IN_PROCESS:
      return gpu::mojom::CommandBufferNamespace::IN_PROCESS;
    case gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE:
      return gpu::mojom::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE;
    case gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL:
      return gpu::mojom::CommandBufferNamespace::
          VIZ_SKIA_OUTPUT_SURFACE_NON_DDL;
    case gpu::CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE:
      return gpu::mojom::CommandBufferNamespace::
          GPU_CHANNEL_SHARED_IMAGE_INTERFACE;
    case gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE:
      return gpu::mojom::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE;
    case gpu::CommandBufferNamespace::NUM_COMMAND_BUFFER_NAMESPACES:
      return gpu::mojom::CommandBufferNamespace::INVALID;
  }
}

// static
bool EnumTraits<gpu::mojom::CommandBufferNamespace,
                gpu::CommandBufferNamespace>::
    FromMojom(gpu::mojom::CommandBufferNamespace input,
              gpu::CommandBufferNamespace* out) {
  switch (input) {
    case gpu::mojom::CommandBufferNamespace::INVALID:
      *out = gpu::CommandBufferNamespace::INVALID;
      return true;
    case gpu::mojom::CommandBufferNamespace::GPU_IO:
      *out = gpu::CommandBufferNamespace::GPU_IO;
      return true;
    case gpu::mojom::CommandBufferNamespace::IN_PROCESS:
      *out = gpu::CommandBufferNamespace::IN_PROCESS;
      return true;
    case gpu::mojom::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE:
      *out = gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE;
      return true;
    case gpu::mojom::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL:
      *out = gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL;
      return true;
    case gpu::mojom::CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE:
      *out = gpu::CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE;
      return true;
    case gpu::mojom::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE:
      *out = gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE;
      return true;
  }
  return false;
}

}  // namespace mojo
