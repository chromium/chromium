// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/video_effects/calculators/inference_calculator_webgpu.h"

#include <memory>
#include <optional>

#include "base/no_destructor.h"
#include "base/notreached.h"
#include "services/on_device_model/ml/chrome_ml_api.h"
#include "services/on_device_model/ml/chrome_ml_holder.h"
#include "services/video_effects/calculators/mediapipe_webgpu_utils.h"
#include "services/video_effects/calculators/video_effects_graph_config.h"
#include "third_party/abseil-cpp/absl/status/status.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "third_party/dawn/include/dawn/webgpu_cpp.h"
#include "third_party/dawn/include/dawn/wire/WireClient.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_context.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_contract.h"
#include "third_party/mediapipe/src/mediapipe/framework/calculator_registry.h"
#include "third_party/mediapipe/src/mediapipe/framework/packet.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer.h"
#include "third_party/mediapipe/src/mediapipe/gpu/gpu_buffer_format.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_service.h"
#include "third_party/mediapipe/src/mediapipe/gpu/webgpu/webgpu_texture_view.h"

namespace {

constexpr uint32_t kBufferWidth = video_effects::kInferenceInputBufferWidth;
constexpr uint32_t kBufferHeight = video_effects::kInferenceInputBufferHeight;
constexpr mediapipe::GpuBufferFormat kBufferFormat =
    video_effects::WebGpuTextureFormatToGpuBufferFormat(
        video_effects::kInferenceInputBufferFormat);

const ChromeMLAPI* GetChromeMlApi() {
  static base::NoDestructor<std::unique_ptr<ml::ChromeMLHolder>> holder{
      ml::ChromeMLHolder::Create()};

  ml::ChromeMLHolder* holder_ptr = holder->get();
  if (!holder_ptr) {
    return nullptr;
  }

  return &holder_ptr->api();
}

DISABLE_CFI_DLSYM
void ChromeMLFatalErrorFnImpl(const char* msg) {
  NOTREACHED() << "ChromeMFatalErrorFn invoked, msg=" << msg;
}

}  // namespace

namespace video_effects {

InferenceCalculatorWebGpu::InferenceCalculatorWebGpu() = default;
InferenceCalculatorWebGpu::~InferenceCalculatorWebGpu() = default;

// static
absl::Status InferenceCalculatorWebGpu::GetContract(
    mediapipe::CalculatorContract* cc) {
  cc->UseService(mediapipe::kWebGpuService);

  cc->InputSidePackets()
      .Tag(kStaticConfigInputSidePacketStreamTag)
      .Set<video_effects::StaticConfig>();

  cc->Inputs()
      .Tag(kRuntimeConfigInputStreamTag)
      .Set<video_effects::RuntimeConfig>();
  cc->Inputs().Tag(kInputTextureStreamTag).Set<mediapipe::GpuBuffer>();

  cc->Outputs().Tag(kOutputTextureStreamTag).Set<mediapipe::GpuBuffer>();

  return absl::OkStatus();
}

DISABLE_CFI_DLSYM absl::Status InferenceCalculatorWebGpu::Open(
    mediapipe::CalculatorContext* cc) {
  auto* ml_api = GetChromeMlApi();
  CHECK(ml_api);

  ml_api->InitDawnProcs(dawn::wire::client::GetProcs());
  ml_api->SetFatalErrorNonGpuFn(&ChromeMLFatalErrorFnImpl);
  ml_api->SetFatalErrorFn(&ChromeMLFatalErrorFnImpl);

  const mediapipe::Packet& static_config_packet =
      cc->InputSidePackets().Tag(kStaticConfigInputSidePacketStreamTag);
  const StaticConfig& static_config = static_config_packet.Get<StaticConfig>();
  CHECK(!static_config.background_segmentation_model().empty());

  auto device = cc->Service(mediapipe::kWebGpuService).GetObject().device();
  auto adapter = device.GetAdapter();
  wgpu::AdapterInfo adapter_info;
  adapter.GetInfo(&adapter_info);

  inference_engine_ = ml_api->CreateInferenceEngine(
      adapter_info, device.Get(),
      reinterpret_cast<const char*>(
          static_config.background_segmentation_model().data()),
      static_config.background_segmentation_model().size());

  if (!inference_engine_) {
    return absl::InternalError("Failed to create the inference engine!");
  }

  return absl::OkStatus();
}

DISABLE_CFI_DLSYM absl::Status InferenceCalculatorWebGpu::Process(
    mediapipe::CalculatorContext* cc) {
  auto* ml_api = GetChromeMlApi();
  CHECK(ml_api);

  mediapipe::Packet& config_packet =
      cc->Inputs().Tag(kRuntimeConfigInputStreamTag).Value();
  if (config_packet.IsEmpty()) {
    return absl::InternalError("Runtime configuration not present!");
  }

  if (config_packet.Get<RuntimeConfig>().blur_state != BlurState::kEnabled) {
    return absl::InternalError(
        "Blur is disabled, the calculator should not even run!");
  }

  mediapipe::Packet& input_frame =
      cc->Inputs().Tag(kInputTextureStreamTag).Value();
  mediapipe::GpuBuffer input_buffer = input_frame.Get<mediapipe::GpuBuffer>();
  auto input_buffer_view =
      input_buffer.GetReadView<mediapipe::WebGpuTextureView>();

  mediapipe::GpuBuffer output_buffer(kBufferWidth, kBufferHeight,
                                     kBufferFormat);
  auto output_buffer_view =
      output_buffer.GetWriteView<mediapipe::WebGpuTextureView>();

  if (!ml_api->RunInference(inference_engine_,
                            input_buffer_view.texture().Get(),
                            output_buffer_view.texture().Get())) {
    return absl::InternalError("Processing failed");
  }
  cc->Outputs()
      .Tag(kOutputTextureStreamTag)
      .AddPacket(
          mediapipe::MakePacket<mediapipe::GpuBuffer>(std::move(output_buffer))
              .At(input_frame.Timestamp()));

  return absl::OkStatus();
}

DISABLE_CFI_DLSYM
absl::Status InferenceCalculatorWebGpu::Close(
    mediapipe::CalculatorContext* cc) {
  auto* ml_api = GetChromeMlApi();
  CHECK(ml_api);

  if (inference_engine_) {
    ml_api->DestroyInferenceEngine(inference_engine_);
    inference_engine_ = 0;
  }

  return absl::OkStatus();
}

REGISTER_CALCULATOR(InferenceCalculatorWebGpu)

}  // namespace video_effects
