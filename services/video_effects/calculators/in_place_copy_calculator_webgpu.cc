// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/calculators/in_place_copy_calculator_webgpu.h"

#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_context.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_contract.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_registry.h"
#include "third_party/mediapipe/src/mediapipe/framework/packet.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_service.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_texture_view.h"

namespace video_effects {

InPlaceCopyCalculatorWebGpu::InPlaceCopyCalculatorWebGpu() = default;
InPlaceCopyCalculatorWebGpu::~InPlaceCopyCalculatorWebGpu() = default;

absl::Status InPlaceCopyCalculatorWebGpu::GetContract(
    mediapipe::CalculatorContract* cc) {
  cc->UseService(mediapipe::kWebGpuService);

  cc->Inputs().Tag(kInputStreamTag).Set<mediapipe::GpuBuffer>();
  cc->Inputs().Tag(kOutputInputStreamTag).Set<mediapipe::GpuBuffer>();

  cc->Outputs().Tag(kOutputStreamTag).Set<mediapipe::GpuBuffer>();
  return absl::OkStatus();
}

absl::Status InPlaceCopyCalculatorWebGpu::Open(
    mediapipe::CalculatorContext* cc) {
  wgpu::Device device =
      cc->Service(mediapipe::kWebGpuService).GetObject().device();
  if (!device) {
    return absl::InternalError(
        "Failed to obtain the WebGPU device from the service!");
  }

  return absl::OkStatus();
}

absl::Status InPlaceCopyCalculatorWebGpu::Process(
    mediapipe::CalculatorContext* cc) {
  wgpu::Device device =
      cc->Service(mediapipe::kWebGpuService).GetObject().device();
  if (!device) {
    return absl::InternalError(
        "Failed to obtain the WebGPU device from the service!");
  }

  mediapipe::Packet& input_packet = cc->Inputs().Tag(kInputStreamTag).Value();
  auto input_buffer = input_packet.Get<mediapipe::GpuBuffer>();
  auto input_texture_view =
      input_buffer.GetReadView<mediapipe::WebGpuTextureView>();

  mediapipe::Packet& output_packet =
      cc->Inputs().Tag(kOutputInputStreamTag).Value();
  auto output_buffer = output_packet.Get<mediapipe::GpuBuffer>();
  auto output_texture_view =
      output_buffer.GetWriteView<mediapipe::WebGpuTextureView>();

  if (input_buffer.width() != output_buffer.width() ||
      input_buffer.height() != output_buffer.height()) {
    return absl::InternalError(
        "Mismatching dimensions of input and output buffers");
  }

  wgpu::CommandEncoder command_encoder = device.CreateCommandEncoder();
  wgpu::TexelCopyTextureInfo source = {.texture = input_texture_view.texture()};
  wgpu::TexelCopyTextureInfo destination = {.texture =
                                                output_texture_view.texture()};
  wgpu::Extent3D extent = {
      .width = static_cast<uint32_t>(input_buffer.width()),
      .height = static_cast<uint32_t>(input_buffer.height()),
      .depthOrArrayLayers = 1};
  command_encoder.CopyTextureToTexture(&source, &destination, &extent);
  wgpu::CommandBufferDescriptor command_buffer_descriptor = {
      .label = "InPlaceCopyCalculator",
  };
  wgpu::CommandBuffer command_buffer =
      command_encoder.Finish(&command_buffer_descriptor);
  device.GetQueue().Submit(1, &command_buffer);
  cc->Outputs().Tag(kOutputStreamTag).AddPacket(output_packet);
  return absl::OkStatus();
}

absl::Status InPlaceCopyCalculatorWebGpu::Close(
    mediapipe::CalculatorContext* cc) {
  return absl::OkStatus();
}

REGISTER_CALCULATOR(InPlaceCopyCalculatorWebGpu)

}  // namespace video_effects
