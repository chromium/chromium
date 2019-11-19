// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/viz/public/cpp/gpu/command_buffer_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "components/viz/common/gpu/context_lost_reason.h"

namespace viz {
namespace command_buffer_metrics {

namespace {

void RecordContextLost(ContextType type, ContextLostReason reason) {
  switch (type) {
    case ContextType::BROWSER_COMPOSITOR:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.BrowserCompositor", reason);
      break;
    case ContextType::BROWSER_MAIN_THREAD:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.BrowserMainThread", reason);
      break;
    case ContextType::BROWSER_WORKER:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.BrowserWorker", reason);
      break;
    case ContextType::RENDER_COMPOSITOR:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.RenderCompositor", reason);
      break;
    case ContextType::RENDER_WORKER:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.RenderWorker", reason);
      break;
    case ContextType::RENDERER_MAIN_THREAD:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.RenderMainThread", reason);
      break;
    case ContextType::VIDEO_ACCELERATOR:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.VideoAccelerator", reason);
      break;
    case ContextType::VIDEO_CAPTURE:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.VideoCapture", reason);
      break;
    case ContextType::WEBGL:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.WebGL", reason);
      break;
    case ContextType::WEBGPU:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.WebGPU", reason);
      break;
    case ContextType::MEDIA:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.Media", reason);
      break;
    case ContextType::UNKNOWN:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.Unknown", reason);
      break;
    case ContextType::FOR_TESTING:
      // Don't record UMA, this is just for tests.
      break;
    case ContextType::XR_COMPOSITING:
      UMA_HISTOGRAM_ENUMERATION("GPU.ContextLost.XRCompositing", reason);
      break;
  }
}

}  // anonymous namespace

std::string ContextTypeToString(ContextType type) {
  switch (type) {
    case ContextType::BROWSER_COMPOSITOR:
      return "BrowserCompositor";
    case ContextType::BROWSER_MAIN_THREAD:
      return "BrowserMainThread";
    case ContextType::BROWSER_WORKER:
      return "BrowserWorker";
    case ContextType::RENDER_COMPOSITOR:
      return "RenderCompositor";
    case ContextType::RENDER_WORKER:
      return "RenderWorker";
    case ContextType::RENDERER_MAIN_THREAD:
      return "RendererMainThread";
    case ContextType::VIDEO_ACCELERATOR:
      return "VideoAccelerator";
    case ContextType::VIDEO_CAPTURE:
      return "VideoCapture";
    case ContextType::WEBGL:
      return "WebGL";
    case ContextType::WEBGPU:
      return "WebGPU";
    case ContextType::MEDIA:
      return "Media";
    case ContextType::UNKNOWN:
      return "Unknown";
    case ContextType::FOR_TESTING:
      return "ForTesting";
    case ContextType::XR_COMPOSITING:
      return "XRCompositing";
  }
}

void UmaRecordContextInitFailed(ContextType type) {
  RecordContextLost(type, CONTEXT_INIT_FAILED);
}

void UmaRecordContextLost(ContextType type,
                          gpu::error::Error error,
                          gpu::error::ContextLostReason reason) {
  ContextLostReason converted_reason = GetContextLostReason(error, reason);
  RecordContextLost(type, converted_reason);
}

}  // namespace command_buffer_metrics
}  // namespace viz
