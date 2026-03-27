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
gpu::error::Error EnumTraits<gpu::mojom::Error, gpu::error::Error>::FromMojom(
    gpu::mojom::Error input) {
  switch (input) {
    case gpu::mojom::Error::kNoError:
      return gpu::error::kNoError;
    case gpu::mojom::Error::kInvalidSize:
      return gpu::error::kInvalidSize;
    case gpu::mojom::Error::kOutOfBounds:
      return gpu::error::kOutOfBounds;
    case gpu::mojom::Error::kUnknownCommand:
      return gpu::error::kUnknownCommand;
    case gpu::mojom::Error::kInvalidArguments:
      return gpu::error::kInvalidArguments;
    case gpu::mojom::Error::kLostContext:
      return gpu::error::kLostContext;
    case gpu::mojom::Error::kGenericError:
      return gpu::error::kGenericError;
    case gpu::mojom::Error::kDeferCommandUntilLater:
      return gpu::error::kDeferCommandUntilLater;
    case gpu::mojom::Error::kDeferLaterCommands:
      return gpu::error::kDeferLaterCommands;
  }
  NOTREACHED();
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
gpu::error::ContextLostReason
EnumTraits<gpu::mojom::ContextLostReason, gpu::error::ContextLostReason>::
    FromMojom(gpu::mojom::ContextLostReason input) {
  switch (input) {
    case gpu::mojom::ContextLostReason::kGuilty:
      return gpu::error::kGuilty;
    case gpu::mojom::ContextLostReason::kInnocent:
      return gpu::error::kInnocent;
    case gpu::mojom::ContextLostReason::kUnknown:
      return gpu::error::kUnknown;
    case gpu::mojom::ContextLostReason::kOutOfMemory:
      return gpu::error::kOutOfMemory;
    case gpu::mojom::ContextLostReason::kMakeCurrentFailed:
      return gpu::error::kMakeCurrentFailed;
    case gpu::mojom::ContextLostReason::kGpuChannelLost:
      return gpu::error::kGpuChannelLost;
    case gpu::mojom::ContextLostReason::kInvalidGpuMessage:
      return gpu::error::kInvalidGpuMessage;
  }
  NOTREACHED();
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
gpu::CommandBufferNamespace
EnumTraits<gpu::mojom::CommandBufferNamespace, gpu::CommandBufferNamespace>::
    FromMojom(gpu::mojom::CommandBufferNamespace input) {
  switch (input) {
    case gpu::mojom::CommandBufferNamespace::INVALID:
      return gpu::CommandBufferNamespace::INVALID;
    case gpu::mojom::CommandBufferNamespace::GPU_IO:
      return gpu::CommandBufferNamespace::GPU_IO;
    case gpu::mojom::CommandBufferNamespace::IN_PROCESS:
      return gpu::CommandBufferNamespace::IN_PROCESS;
    case gpu::mojom::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE:
      return gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE;
    case gpu::mojom::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL:
      return gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE_NON_DDL;
    case gpu::mojom::CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE:
      return gpu::CommandBufferNamespace::GPU_CHANNEL_SHARED_IMAGE_INTERFACE;
    case gpu::mojom::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE:
      return gpu::CommandBufferNamespace::WEBNN_CONTEXT_INTERFACE;
  }
  NOTREACHED();
}

}  // namespace mojo
