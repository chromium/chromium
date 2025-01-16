// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/calculators/video_effects_graph_webgpu.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator.pb.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_graph.h"
#include "third_party/mediapipe/src/mediapipe/framework/packet.h"
#include "third_party/mediapipe/src/mediapipe/framework/timestamp.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer_format.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_texture_buffer.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_texture_view.h"

namespace video_effects {

namespace {

mediapipe::GpuBufferFormat WebGpuTextureFormatToGpuBufferFormat(
    wgpu::TextureFormat format) {
  switch (format) {
    case wgpu::TextureFormat::RGBA8Unorm:
      return mediapipe::GpuBufferFormat::kRGBA32;
    case wgpu::TextureFormat::RGBA32Float:
      return mediapipe::GpuBufferFormat::kRGBAFloat128;
    default:
      NOTREACHED();
  }
}

}  // namespace

// Inputs:
constexpr char kStaticConfigInputStreamName[] = "static_config";
constexpr char kRuntimeConfigInputStreamName[] = "runtime_config";
constexpr char kInputTextureInputStreamName[] = "input_texture";
constexpr char kInputTextureDownscaledInputStreamName[] =
    "input_texture_downscaled";
constexpr char kOutputTextureInputStreamName[] = "output_texture";

// Outputs:
constexpr char kBackgroundMaskOutputStreamName[] = "mask";
constexpr char kOutputTextureOutputStreamName[] = "out";

VideoEffectsGraphWebGpu::~VideoEffectsGraphWebGpu() = default;

VideoEffectsGraphWebGpu::VideoEffectsGraphWebGpu(
    std::unique_ptr<mediapipe::CalculatorGraph> graph)
    : graph_(std::move(graph)) {}

// static
std::unique_ptr<VideoEffectsGraphWebGpu> VideoEffectsGraphWebGpu::Create() {
  mediapipe::CalculatorGraphConfig config;
  // Inputs for the entire graph:
  config.add_input_side_packet(kStaticConfigInputStreamName);
  config.add_input_stream(kRuntimeConfigInputStreamName);
  config.add_input_stream(kInputTextureInputStreamName);
  config.add_input_stream(kInputTextureDownscaledInputStreamName);
  // Note: output texture is also provided as an input, the graph will be
  // pouplating those with contents:
  config.add_input_stream(kOutputTextureInputStreamName);
  config.add_output_stream(kOutputTextureOutputStreamName);

  auto* inference_node = config.add_node();
  inference_node->set_calculator("InferenceCalculatorWebGpu");
  // Inputs for inference calculator node:
  inference_node->add_input_side_packet(kStaticConfigInputStreamName);
  inference_node->add_input_stream(kRuntimeConfigInputStreamName);
  inference_node->add_input_stream(kInputTextureDownscaledInputStreamName);
  inference_node->add_output_stream(kBackgroundMaskOutputStreamName);

  auto* blur_node = config.add_node();
  blur_node->set_calculator("BackgroundBlurCalculatorWebGpu");
  blur_node->add_input_side_packet(kStaticConfigInputStreamName);
  blur_node->add_input_stream(kRuntimeConfigInputStreamName);
  blur_node->add_input_stream(kInputTextureInputStreamName);
  blur_node->add_input_stream(kBackgroundMaskOutputStreamName);
  blur_node->add_input_stream(kOutputTextureInputStreamName);
  blur_node->add_output_stream(kOutputTextureOutputStreamName);

  // Dawn Wire over command buffer is not thread-safe, so we need to make
  // MediaPipe use our own thread:
  auto* executor = config.add_executor();
  executor->set_type("ApplicationThreadExecutor");

  auto graph = std::make_unique<mediapipe::CalculatorGraph>();
  absl::Status status = graph->Initialize(config);
  if (!status.ok()) {
    return nullptr;
  }

  return base::WrapUnique(new VideoEffectsGraphWebGpu(std::move(graph)));
}

void VideoEffectsGraphWebGpu::OnFrameProcessed(
    const mediapipe::Packet& packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto output_buffer = packet.Get<mediapipe::GpuBuffer>();
  auto output_texture_view =
      output_buffer.GetReadView<mediapipe::WebGpuTextureView>();

  CHECK(on_frame_callback_);
  on_frame_callback_.Run(output_texture_view.texture());
}

bool VideoEffectsGraphWebGpu::Start(
    StaticConfig static_config,
    base::RepeatingCallback<void(wgpu::Texture)> on_frame_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  on_frame_callback_ = std::move(on_frame_cb);

  auto graph_cb =
      base::BindRepeating(&VideoEffectsGraphWebGpu::OnFrameProcessed,
                          weak_ptr_factory_.GetWeakPtr());
  absl::Status status = graph_->ObserveOutputStream(
      kOutputTextureOutputStreamName,
      [cb = std::move(graph_cb)](const mediapipe::Packet& packet) {
        cb.Run(packet);
        return absl::OkStatus();
      });

  status = graph_->StartRun(
      {{kStaticConfigInputStreamName,
        mediapipe::MakePacket<StaticConfig>(std::move(static_config))}});
  if (!status.ok()) {
    return false;
  }

  return true;
}

bool VideoEffectsGraphWebGpu::ProcessFrame(
    base::TimeDelta timedelta,
    wgpu::Texture input_texture,
    wgpu::Texture input_texture_downscaled,
    wgpu::Texture output_texture,
    const RuntimeConfig& runtime_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const mediapipe::Timestamp ts =
      mediapipe::Timestamp::FromSeconds(timedelta.InSecondsF());

  mediapipe::GpuBuffer input_texture_buffer(
      std::make_shared<mediapipe::WebGpuTextureBuffer>(
          input_texture, input_texture.GetWidth(), input_texture.GetHeight(),
          WebGpuTextureFormatToGpuBufferFormat(input_texture.GetFormat())));
  mediapipe::GpuBuffer input_texture_downscaled_buffer(
      std::make_shared<mediapipe::WebGpuTextureBuffer>(
          input_texture_downscaled, input_texture_downscaled.GetWidth(),
          input_texture_downscaled.GetHeight(),
          WebGpuTextureFormatToGpuBufferFormat(
              input_texture_downscaled.GetFormat())));
  mediapipe::GpuBuffer output_texture_buffer(
      std::make_shared<mediapipe::WebGpuTextureBuffer>(
          output_texture, output_texture.GetWidth(), output_texture.GetHeight(),
          WebGpuTextureFormatToGpuBufferFormat(output_texture.GetFormat())));

  absl::Status status = graph_->AddPacketToInputStream(
      kInputTextureInputStreamName, mediapipe::MakePacket<mediapipe::GpuBuffer>(
                                        std::move(input_texture_buffer))
                                        .At(ts));
  if (!status.ok()) {
    return false;
  }

  status = graph_->AddPacketToInputStream(
      kInputTextureDownscaledInputStreamName,
      mediapipe::MakePacket<mediapipe::GpuBuffer>(
          std::move(input_texture_downscaled_buffer))
          .At(ts));
  if (!status.ok()) {
    return false;
  }

  status = graph_->AddPacketToInputStream(
      kOutputTextureInputStreamName,
      mediapipe::MakePacket<mediapipe::GpuBuffer>(
          std::move(output_texture_buffer))
          .At(ts));
  if (!status.ok()) {
    return false;
  }

  status = graph_->AddPacketToInputStream(
      kRuntimeConfigInputStreamName,
      mediapipe::MakePacket<RuntimeConfig>(runtime_config).At(ts));

  return status.ok();
}

bool VideoEffectsGraphWebGpu::WaitUntilIdle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return graph_->WaitUntilIdle().ok();
}

}  // namespace video_effects
