// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/calculators/background_blur_calculator_webgpu.h"

#include <numbers>
#include <sstream>
#include <tuple>
#include <vector>

#include "base/check_op.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_context.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_contract.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_registry.h"
#include "third_party/mediapipe/src/mediapipe/framework/packet.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_service.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_texture_view.h"

namespace video_effects {

namespace {

// The size of a single dimension of a workgroup for the background blur compute
// shader. WebGPU guarantees that all implementations should support at least
// 16x16 workgroup sizes.
constexpr uint32_t kTileSize = 16;

// Computes a matrix containing 2d Gaussian blur kernel
// (see formula at https://en.wikipedia.org/wiki/Gaussian_blur).
// The resulting matrix will have dimensions NxN where `N = 2 * radius - 1`.
// Note1: this function can _almost_ be constexpr, except for the fact that
// `std::exp()` is not constexpr until C++26.
// Note2: we could tie the value of `radius` and `std_dev` together. Per
// Wikipedia: "pixels at a distance of more than 3*std_dev have a small enough
// influence to be considered effectively zero".
std::vector<std::vector<double>> CalculateGaussianBlur(uint32_t radius,
                                                       double std_dev) {
  CHECK_GT(radius, 0u);

  const double variance = std_dev * std_dev;
  const uint32_t diameter = 2 * radius - 1;

  // Coordinates of the matrix's center:
  const std::tuple<uint32_t, uint32_t> center =
      std::make_tuple(radius - 1, radius - 1);

  std::vector<std::vector<double>> result(diameter,
                                          std::vector<double>(diameter, 0.0));
  for (auto row = 0u; row < diameter; ++row) {
    for (auto col = 0u; col < diameter; ++col) {
      const int dx = std::get<0>(center) - col;
      const int dy = std::get<1>(center) - row;

      const double coefficient = 1.0 / (2 * std::numbers::pi * variance);
      const double exponent = -(dx * dx + dy * dy) / (2 * variance);

      result[row][col] = coefficient * std::exp(exponent);
    }
  }

  return result;
}

// Given a NxN matrix, produce a WGSL string that creates a variable containing
// the matrix. This string can then be pasted into WGSL shader source code.
std::string WgslString(const std::string& variable_name,
                       const std::vector<std::vector<double>>& matrix) {
  // Matrix has at least 1 row:
  CHECK(!matrix.empty());
  // Matrix has at least 1 column:
  CHECK(!matrix[0].empty());
  // Matrix is square:
  CHECK_EQ(matrix.size(), matrix[0].size());

  const uint32_t size = matrix.size();

  std::stringstream initializer;
  for (auto row = 0u; row < matrix.size(); ++row) {
    initializer << "array(";
    for (auto col = 0u; col < matrix[row].size(); ++col) {
      initializer << matrix[row][col];
      if (col + 1 != matrix[row].size()) {
        initializer << ", ";
      }
    }
    initializer << ")";
    if (row + 1 != matrix.size()) {
      initializer << ",\n";
    }
  }

  std::string result =
      base::StringPrintf("const %s: array<array<f32, %u>, %u> = array(%s);",
                         variable_name, size, size, initializer.str());
  return result;
}

}  // namespace

BackgroundBlurCalculatorWebGpu::BackgroundBlurCalculatorWebGpu() = default;
BackgroundBlurCalculatorWebGpu::~BackgroundBlurCalculatorWebGpu() = default;

absl::Status BackgroundBlurCalculatorWebGpu::GetContract(
    mediapipe::CalculatorContract* cc) {
  cc->UseService(mediapipe::kWebGpuService);

  cc->Inputs()
      .Tag(kRuntimeConfigInputStreamTag)
      .Set<video_effects::RuntimeConfig>();
  cc->Inputs()
      .Tag(kInputTextureStreamTag)
      .Set<mediapipe::GpuBuffer>();  // original
  cc->Inputs().Tag(kMaskTextureStreamTag).Set<mediapipe::GpuBuffer>();  // mask

  cc->Outputs().Tag(kOutputTextureStreamTag).Set<mediapipe::GpuBuffer>();

  return absl::OkStatus();
}

absl::Status BackgroundBlurCalculatorWebGpu::Open(
    mediapipe::CalculatorContext* cc) {
  wgpu::Device device =
      cc->Service(mediapipe::kWebGpuService).GetObject().device();
  if (!device) {
    return absl::InternalError(
        "Failed to obtain the WebGPU device from the service!");
  }

  std::vector<wgpu::BindGroupLayoutEntry> bindings;
  // Input frame:
  bindings.push_back({
      .binding = 0,
      .visibility = wgpu::ShaderStage::Compute,
      .texture =
          {
              .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
              .viewDimension = wgpu::TextureViewDimension::e2D,
              .multisampled = false,
          },
  });
  // Input mask:
  bindings.push_back({
      .binding = 1,
      .visibility = wgpu::ShaderStage::Compute,
      .texture =
          {
              .sampleType = wgpu::TextureSampleType::UnfilterableFloat,
              .viewDimension = wgpu::TextureViewDimension::e2D,
              .multisampled = false,
          },
  });
  // Output frame:
  bindings.push_back({
      .binding = 2,
      .visibility = wgpu::ShaderStage::Compute,
      .storageTexture =
          {
              .access = wgpu::StorageTextureAccess::WriteOnly,
              .format = wgpu::TextureFormat::RGBA8Unorm,
              .viewDimension = wgpu::TextureViewDimension::e2D,
          },
  });

  wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor = {
      .label = "BackgroundBlurCalculatorWebGpuBindGroupLayout",
      .entryCount = bindings.size(),
      .entries = bindings.data(),
  };

  wgpu::BindGroupLayout bind_group_layout =
      device.CreateBindGroupLayout(&bind_group_layout_descriptor);
  wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor = {
      .label = "BackgroundBlurCalculatorWebGpuComputeShader",
      .bindGroupLayoutCount = 1,
      .bindGroupLayouts = &bind_group_layout,
  };
  wgpu::PipelineLayout compute_pipeline_layout =
      device.CreatePipelineLayout(&pipeline_layout_descriptor);

  constexpr float kBlurStdDev = 1.5;
  // Per Wikipedia, we could use 6sigma x 6sigma matrix here.
  // 7x7 matrix seems to be good enough:
  constexpr uint32_t kBlurRadius = 4;

  static const std::string kBlurMatrix =
      WgslString("kKernel", CalculateGaussianBlur(kBlurRadius, kBlurStdDev));

  // TODO(http://b/384567251): Gaussian blur is separable, therefore it may be
  // beneficial to convert the blur logic into a 2-pass computation for reduced
  // number of texture reads.
  constexpr char kShaderCodeTemplate[] = R"(
%s

const kTileSize = %u;
const kKernelLength = %u;

// Set to true to just visualize the mask:
const DEBUG_MASK: bool = false;

@group(0) @binding(0) var inputBuffer: texture_2d<f32>;
@group(0) @binding(1) var inputMask: texture_2d<f32>;
@group(0) @binding(2) var outputBuffer: texture_storage_2d<rgba8unorm, write>;

@compute @workgroup_size(kTileSize, kTileSize, 1)
fn blur(@builtin(global_invocation_id) id: vec3<u32>) {
  let inputSize: vec2<u32> = textureDimensions(inputBuffer);
  let maskSize: vec2<u32> = textureDimensions(inputMask);
  let outputSize: vec2<u32> = textureDimensions(outputBuffer);

  let outputPosition: vec2<u32> = id.xy;
  var maskPosition: vec2<u32> = vec2<u32>(
    (vec2<f32>(outputPosition) / vec2<f32>(outputSize)) * vec2<f32>(maskSize));

  // We only have things to do if the mask coordinates all fall under the mask,
  // (i.e. `maskPosition.x < maskSize.x && maskPosition.y < maskSize.y`).
  // `all()` will ensure that this is the case.
  if (all(maskPosition < maskSize)) {
    let mask: f32 =  textureLoad(inputMask, maskPosition, 0).r;

    if (DEBUG_MASK) {
      textureStore(outputBuffer, outputPosition, vec4f(mask, mask, mask, 1.0));
      return;
    }

    // TODO(http://b/384567251): experiment with the logic here.
    // The higher the step, the further away from the original pixel we'll look.
    // The step will increase the less confident we are that we're processing a
    // foreground pixel:
    var step = 0.0;
    if(mask <= 0.25) {
      step = 2.5;
    } else {
      step = 1.5;
    }

    var color: vec4<f32> = vec4<f32>();
    if(mask >= 0.5) {
      // We're pretty confident this is the foreground, keep the original color:
      color = textureLoad(inputBuffer, outputPosition, 0);
    } else {
      // Apply blur.
      var newColor = vec4<f32>();
      for(var row: u32 = 0; row < kKernelLength; row++) {
        for(var col: u32 = 0; col < kKernelLength; col++) {
          // Transform (col, row) coordinates (relative to the corner of the
          // kernel) into coordinates relative to the middle of the kernel:
          let dx = f32(col) - f32(kKernelLength)/2;  // [0, n) -> [-n/2, n/2)
          let dy = f32(row) - f32(kKernelLength)/2;

          let delta = vec2<i32>(round(vec2<f32>(dx, dy) * vec2<f32>(step)));
          // TODO(http://b/384567251): probably we want to use a sampler here:
          let inputCoords = clamp(
            vec2<i32>(outputPosition) + delta,
            vec2<i32>(), vec2<i32>(inputSize));
          let c = textureLoad(inputBuffer, inputCoords, 0);
          newColor += c * kKernel[row][col];
        }
      }

      color = newColor;
    }

    textureStore(outputBuffer, outputPosition, color);
  }
}
)";

  const std::string kShaderCode = base::StringPrintf(
      kShaderCodeTemplate, kBlurMatrix, kTileSize, kBlurRadius * 2 - 1);

  wgpu::ShaderSourceWGSL compute_shader_wgsl_descriptor;
  compute_shader_wgsl_descriptor.code = kShaderCode.c_str();
  wgpu::ShaderModuleDescriptor compute_shader_descriptor = {
      .nextInChain = &compute_shader_wgsl_descriptor,
      .label = "BackgroundBlurCalculatorWebGpuComputeShader",
  };
  wgpu::ShaderModule compute_shader =
      device.CreateShaderModule(&compute_shader_descriptor);

  wgpu::ComputePipelineDescriptor compute_pipeline_descriptor = {
      .label = "BackgroundBlurCalculatorWebGpuComputePipeline",
      .layout = compute_pipeline_layout,
      .compute =
          {
              .module = compute_shader,
              .entryPoint = "blur",
          },
  };

  compute_pipeline_ =
      device.CreateComputePipeline(&compute_pipeline_descriptor);

  return absl::OkStatus();
}

absl::Status BackgroundBlurCalculatorWebGpu::Process(
    mediapipe::CalculatorContext* cc) {
  wgpu::Device device =
      cc->Service(mediapipe::kWebGpuService).GetObject().device();
  if (!device) {
    return absl::InternalError(
        "Failed to obtain the WebGPU device from the service!");
  }

  CHECK(compute_pipeline_);

  mediapipe::Packet& config_packet =
      cc->Inputs().Tag(kRuntimeConfigInputStreamTag).Value();
  if (config_packet.IsEmpty()) {
    return absl::InternalError("Runtime configuration not present!");
  }

  if (config_packet.Get<RuntimeConfig>().blur_state != BlurState::kEnabled) {
    return absl::InternalError(
        "Blur is disabled, the calculator should not even run!");
  }

  mediapipe::Packet& input_packet =
      cc->Inputs().Tag(kInputTextureStreamTag).Value();
  auto input_buffer = input_packet.Get<mediapipe::GpuBuffer>();
  auto input_texture_view =
      input_buffer.GetReadView<mediapipe::WebGpuTextureView>();

  mediapipe::GpuBuffer output_buffer(
      input_buffer.width(), input_buffer.height(), input_buffer.format());
  auto output_texture_view =
      output_buffer.GetWriteView<mediapipe::WebGpuTextureView>();

  // Mask can only be accessed if the previous calculator produced it, and it
  // should have done so iff runtime config was enabled:
  mediapipe::Packet& mask_packet =
      cc->Inputs().Tag(kMaskTextureStreamTag).Value();
  auto mask_buffer = mask_packet.Get<mediapipe::GpuBuffer>();
  auto mask_texture_view =
      mask_buffer.GetReadView<mediapipe::WebGpuTextureView>();

  std::vector<wgpu::BindGroupEntry> bind_group_entries;
  bind_group_entries.push_back({
      .binding = 0,
      .textureView = input_texture_view.texture().CreateView(),
  });

  bind_group_entries.push_back({
      .binding = 1,
      .textureView = mask_texture_view.texture().CreateView(),
  });

  bind_group_entries.push_back({
      .binding = 2,
      .textureView = output_texture_view.texture().CreateView(),
  });

  wgpu::BindGroupDescriptor bind_group_descriptor = {
      .label = "BackgroundBlurCalculatorWebGpuBindGroup",
      .layout = compute_pipeline_.GetBindGroupLayout(0),
      .entryCount = bind_group_entries.size(),
      .entries = bind_group_entries.data(),
  };
  wgpu::BindGroup bind_group = device.CreateBindGroup(&bind_group_descriptor);

  wgpu::CommandEncoder command_encoder = device.CreateCommandEncoder();
  wgpu::ComputePassEncoder compute_pass_encoder =
      command_encoder.BeginComputePass();
  compute_pass_encoder.SetPipeline(compute_pipeline_);
  compute_pass_encoder.SetBindGroup(/*groupIndex=*/0, bind_group);
  // TODO(http://b/384567251): Experiment with dynamically picking tile size
  // based on the size of the input video frame.
  // Dispatch the shader for each pixel in the video frame. Since what gets
  // dispatched is an entire workgroup, we need to compute the number of
  // workgroups by dividing the video frame dimensions by workgroup dimensions.
  // Note: this needs to match the workgroup size in the shader source:
  compute_pass_encoder.DispatchWorkgroups(
      base::ClampCeil(input_buffer.width() / static_cast<float>(kTileSize)),
      base::ClampCeil(input_buffer.height() / static_cast<float>(kTileSize)));
  compute_pass_encoder.End();

  wgpu::CommandBufferDescriptor command_buffer_descriptor = {
      .label = "BackgroundBlurCalculatorWebGpuCommandBuffer",
  };
  wgpu::CommandBuffer command_buffer =
      command_encoder.Finish(&command_buffer_descriptor);

  device.GetQueue().Submit(/*commandCount=*/1, &command_buffer);

  cc->Outputs()
      .Tag(kOutputTextureStreamTag)
      .AddPacket(
          mediapipe::MakePacket<mediapipe::GpuBuffer>(std::move(output_buffer))
              .At(input_packet.Timestamp()));

  return absl::OkStatus();
}

absl::Status BackgroundBlurCalculatorWebGpu::Close(
    mediapipe::CalculatorContext* cc) {
  compute_pipeline_ = nullptr;

  return absl::OkStatus();
}

REGISTER_CALCULATOR(BackgroundBlurCalculatorWebGpu)

}  // namespace video_effects
