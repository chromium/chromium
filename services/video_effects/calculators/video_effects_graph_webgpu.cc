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
#include "base/strings/strcat.h"
#include "base/task/bind_post_task.h"
#include "services/video_effects/calculators/background_blur_calculator_webgpu.h"
#include "services/video_effects/calculators/downscale_calculator_webgpu.h"
#include "services/video_effects/calculators/in_place_copy_calculator_webgpu.h"
#include "services/video_effects/calculators/inference_calculator_webgpu.h"
#include "services/video_effects/calculators/mediapipe_webgpu_utils.h"
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

std::string StreamFromTagAndName(std::string_view tag,
                                 std::string_view stream_name) {
  return base::StrCat({tag, ":", stream_name});
}

}  // namespace

// Graph inputs:
constexpr char kStaticConfigInputStreamName[] = "static_config";
constexpr char kRuntimeConfigInputStreamName[] = "runtime_config";
constexpr char kInputTextureInputStreamName[] = "input_texture";
constexpr char kOutputTextureInputStreamName[] = "output_texture";

// Intermediate streams:
constexpr char kDownscaledTextureStreamName[] = "downscaled_texture";
constexpr char kBackgroundMaskStreamName[] = "mask";
constexpr char kBluredTextureStreamName[] = "blurred_texture";

// Graph outputs:
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
  // Note: output texture is also provided as an input, the graph will be
  // pouplating those with contents:
  config.add_input_stream(kOutputTextureInputStreamName);
  config.add_output_stream(kOutputTextureOutputStreamName);

  auto* downscale_node = config.add_node();
  downscale_node->set_calculator(DownscaleCalculatorWebGpu::kCalculatorName);
  downscale_node->add_input_stream(
      StreamFromTagAndName(DownscaleCalculatorWebGpu::kInputStreamTag,
                           kInputTextureInputStreamName));
  downscale_node->add_output_stream(
      StreamFromTagAndName(DownscaleCalculatorWebGpu::kOutputStreamTag,
                           kDownscaledTextureStreamName));

  auto* inference_node = config.add_node();
  inference_node->set_calculator(InferenceCalculatorWebGpu::kCalculatorName);
  // Inputs for inference calculator node:
  inference_node->add_input_side_packet(StreamFromTagAndName(
      InferenceCalculatorWebGpu::kStaticConfigInputSidePacketStreamTag,
      kStaticConfigInputStreamName));
  inference_node->add_input_stream(StreamFromTagAndName(
      InferenceCalculatorWebGpu::kRuntimeConfigInputStreamTag,
      kRuntimeConfigInputStreamName));
  inference_node->add_input_stream(
      StreamFromTagAndName(InferenceCalculatorWebGpu::kInputTextureStreamTag,
                           kDownscaledTextureStreamName));
  inference_node->add_output_stream(
      StreamFromTagAndName(InferenceCalculatorWebGpu::kOutputTextureStreamTag,
                           kBackgroundMaskStreamName));

  auto* blur_node = config.add_node();
  blur_node->set_calculator(BackgroundBlurCalculatorWebGpu::kCalculatorName);
  blur_node->add_input_stream(StreamFromTagAndName(
      BackgroundBlurCalculatorWebGpu::kRuntimeConfigInputStreamTag,
      kRuntimeConfigInputStreamName));
  blur_node->add_input_stream(StreamFromTagAndName(
      BackgroundBlurCalculatorWebGpu::kInputTextureStreamTag,
      kInputTextureInputStreamName));
  blur_node->add_input_stream(StreamFromTagAndName(
      BackgroundBlurCalculatorWebGpu::kMaskTextureStreamTag,
      kBackgroundMaskStreamName));

  blur_node->add_output_stream(StreamFromTagAndName(
      BackgroundBlurCalculatorWebGpu::kOutputTextureStreamTag,
      kBluredTextureStreamName));

  auto* in_place_copy_node = config.add_node();
  in_place_copy_node->set_calculator(
      InPlaceCopyCalculatorWebGpu::kCalculatorName);
  in_place_copy_node->add_input_stream(StreamFromTagAndName(
      InPlaceCopyCalculatorWebGpu::kInputStreamTag, kBluredTextureStreamName));
  in_place_copy_node->add_input_stream(
      StreamFromTagAndName(InPlaceCopyCalculatorWebGpu::kOutputInputStreamTag,
                           kOutputTextureInputStreamName));
  in_place_copy_node->add_output_stream(
      StreamFromTagAndName(InPlaceCopyCalculatorWebGpu::kOutputStreamTag,
                           kOutputTextureOutputStreamName));

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
    wgpu::Texture output_texture,
    const RuntimeConfig& runtime_config) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (runtime_config.blur_state != BlurState::kEnabled) {
    return false;
  }

  const mediapipe::Timestamp ts =
      mediapipe::Timestamp::FromSeconds(timedelta.InSecondsF());

  mediapipe::GpuBuffer input_texture_buffer(
      std::make_shared<mediapipe::WebGpuTextureBuffer>(
          input_texture, input_texture.GetWidth(), input_texture.GetHeight(),
          WebGpuTextureFormatToGpuBufferFormat(input_texture.GetFormat())));
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
