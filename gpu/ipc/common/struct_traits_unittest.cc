// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/message_loop/message_loop.h"
#include "build/build_config.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/ipc/common/gpu_feature_info.mojom.h"
#include "gpu/ipc/common/gpu_feature_info_struct_traits.h"
#include "gpu/ipc/common/traits_test_service.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() = default;

 protected:
  mojom::TraitsTestServicePtr GetTraitsTestProxy() {
    mojom::TraitsTestServicePtr proxy;
    traits_test_bindings_.AddBinding(this, mojo::MakeRequest(&proxy));
    return proxy;
  }

 private:
  // TraitsTestService:
  void EchoDxDiagNode(const DxDiagNode& d,
                      EchoDxDiagNodeCallback callback) override {
    std::move(callback).Run(d);
  }

  void EchoGpuDevice(const GPUInfo::GPUDevice& g,
                     EchoGpuDeviceCallback callback) override {
    std::move(callback).Run(g);
  }

  void EchoGpuInfo(const GPUInfo& g, EchoGpuInfoCallback callback) override {
    std::move(callback).Run(g);
  }

  void EchoMailbox(const Mailbox& m, EchoMailboxCallback callback) override {
    std::move(callback).Run(m);
  }

  void EchoMailboxHolder(const MailboxHolder& r,
                         EchoMailboxHolderCallback callback) override {
    std::move(callback).Run(r);
  }

  void EchoSyncToken(const SyncToken& s,
                     EchoSyncTokenCallback callback) override {
    std::move(callback).Run(s);
  }

  void EchoVideoDecodeAcceleratorSupportedProfile(
      const VideoDecodeAcceleratorSupportedProfile& v,
      EchoVideoDecodeAcceleratorSupportedProfileCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoVideoDecodeAcceleratorCapabilities(
      const VideoDecodeAcceleratorCapabilities& v,
      EchoVideoDecodeAcceleratorCapabilitiesCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoVideoEncodeAcceleratorSupportedProfile(
      const VideoEncodeAcceleratorSupportedProfile& v,
      EchoVideoEncodeAcceleratorSupportedProfileCallback callback) override {
    std::move(callback).Run(v);
  }

  void EchoGpuPreferences(const GpuPreferences& prefs,
                          EchoGpuPreferencesCallback callback) override {
    std::move(callback).Run(prefs);
  }

  base::MessageLoop loop_;
  mojo::BindingSet<TraitsTestService> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(StructTraitsTest);
};

}  // namespace

TEST_F(StructTraitsTest, DxDiagNode) {
  gpu::DxDiagNode input;
  input.values["abc"] = "123";
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::DxDiagNode output;
  proxy->EchoDxDiagNode(input, &output);

  gpu::DxDiagNode test_dx_diag_node;
  test_dx_diag_node.values["abc"] = "123";
  EXPECT_EQ(test_dx_diag_node.values, output.values);
}

TEST_F(StructTraitsTest, GPUDevice) {
  gpu::GPUInfo::GPUDevice input;
  // Using the values from gpu/config/gpu_info_collector_unittest.cc::nvidia_gpu
  const uint32_t vendor_id = 0x10de;
  const uint32_t device_id = 0x0df8;
  const std::string vendor_string = "vendor_string";
  const std::string device_string = "device_string";

  input.vendor_id = vendor_id;
  input.device_id = device_id;
  input.vendor_string = vendor_string;
  input.device_string = device_string;
  input.active = false;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::GPUInfo::GPUDevice output;
  proxy->EchoGpuDevice(input, &output);

  EXPECT_EQ(vendor_id, output.vendor_id);
  EXPECT_EQ(device_id, output.device_id);
  EXPECT_FALSE(output.active);
  EXPECT_TRUE(vendor_string.compare(output.vendor_string) == 0);
  EXPECT_TRUE(device_string.compare(output.device_string) == 0);
}

TEST_F(StructTraitsTest, GpuInfo) {
  const base::TimeDelta initialization_time = base::TimeDelta::Max();
  const bool optimus = true;
  const bool amd_switchable = true;
  const gpu::GPUInfo::GPUDevice gpu;
  const std::vector<gpu::GPUInfo::GPUDevice> secondary_gpus;
  const std::string driver_vendor = "driver_vendor";
  const std::string driver_version = "driver_version";
  const std::string driver_date = "driver_date";
  const std::string pixel_shader_version = "pixel_shader_version";
  const std::string vertex_shader_version = "vertex_shader_version";
  const std::string max_msaa_samples = "max_msaa_samples";
  const std::string machine_model_name = "machine_model_name";
  const std::string machine_model_version = "machine_model_version";
  const std::string gl_version = "gl_version";
  const std::string gl_vendor = "gl_vendor";
  const std::string gl_renderer = "gl_renderer";
  const std::string gl_extensions = "gl_extension";
  const std::string gl_ws_vendor = "gl_ws_vendor";
  const std::string gl_ws_version = "gl_ws_version";
  const std::string gl_ws_extensions = "gl_ws_extensions";
  const uint32_t gl_reset_notification_strategy = 0xbeef;
  const bool software_rendering = true;
  const bool direct_rendering = true;
  const bool sandboxed = true;
  const bool in_process_gpu = true;
  const bool passthrough_cmd_decoder = true;
#if defined(OS_WIN)
  const bool direct_composition_overlays = true;
  const gpu::OverlayCapabilities overlay_capabilities = {
      {OverlayFormat::kBGRA, false}, {OverlayFormat::kNV12, true}};
  const DxDiagNode dx_diagnostics;
#endif
  const gpu::VideoDecodeAcceleratorCapabilities
      video_decode_accelerator_capabilities;
  const std::vector<gpu::VideoEncodeAcceleratorSupportedProfile>
      video_encode_accelerator_supported_profiles;
  const bool jpeg_decode_accelerator_supported = true;
#if defined(USE_X11)
  const VisualID system_visual = 0x1234;
  const VisualID rgba_visual = 0x5678;
#endif

  gpu::GPUInfo input;
  input.initialization_time = initialization_time;
  input.optimus = optimus;
  input.amd_switchable = amd_switchable;
  input.gpu = gpu;
  input.secondary_gpus = secondary_gpus;
  input.gpu.driver_vendor = driver_vendor;
  input.gpu.driver_version = driver_version;
  input.gpu.driver_date = driver_date;
  input.pixel_shader_version = pixel_shader_version;
  input.vertex_shader_version = vertex_shader_version;
  input.max_msaa_samples = max_msaa_samples;
  input.machine_model_name = machine_model_name;
  input.machine_model_version = machine_model_version;
  input.gl_version = gl_version;
  input.gl_vendor = gl_vendor;
  input.gl_renderer = gl_renderer;
  input.gl_extensions = gl_extensions;
  input.gl_ws_vendor = gl_ws_vendor;
  input.gl_ws_version = gl_ws_version;
  input.gl_ws_extensions = gl_ws_extensions;
  input.gl_reset_notification_strategy = gl_reset_notification_strategy;
  input.software_rendering = software_rendering;
  input.direct_rendering = direct_rendering;
  input.sandboxed = sandboxed;
  input.in_process_gpu = in_process_gpu;
  input.passthrough_cmd_decoder = passthrough_cmd_decoder;
#if defined(OS_WIN)
  input.direct_composition_overlays = direct_composition_overlays;
  input.overlay_capabilities = overlay_capabilities;
  input.dx_diagnostics = dx_diagnostics;
#endif
  input.video_decode_accelerator_capabilities =
      video_decode_accelerator_capabilities;
  input.video_encode_accelerator_supported_profiles =
      video_encode_accelerator_supported_profiles;
  input.jpeg_decode_accelerator_supported = jpeg_decode_accelerator_supported;
#if defined(USE_X11)
  input.system_visual = system_visual;
  input.rgba_visual = rgba_visual;
#endif

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::GPUInfo output;
  proxy->EchoGpuInfo(input, &output);

  EXPECT_EQ(optimus, output.optimus);
  EXPECT_EQ(amd_switchable, output.amd_switchable);
  EXPECT_EQ(gpu.vendor_id, output.gpu.vendor_id);
  EXPECT_EQ(gpu.device_id, output.gpu.device_id);
  EXPECT_EQ(gpu.active, output.gpu.active);
  EXPECT_EQ(gpu.vendor_string, output.gpu.vendor_string);
  EXPECT_EQ(gpu.device_string, output.gpu.device_string);
  EXPECT_EQ(secondary_gpus.size(), output.secondary_gpus.size());
  for (size_t i = 0; i < secondary_gpus.size(); ++i) {
    const gpu::GPUInfo::GPUDevice& expected_gpu = secondary_gpus[i];
    const gpu::GPUInfo::GPUDevice& actual_gpu = output.secondary_gpus[i];
    EXPECT_EQ(expected_gpu.vendor_id, actual_gpu.vendor_id);
    EXPECT_EQ(expected_gpu.device_id, actual_gpu.device_id);
    EXPECT_EQ(expected_gpu.active, actual_gpu.active);
    EXPECT_EQ(expected_gpu.vendor_string, actual_gpu.vendor_string);
    EXPECT_EQ(expected_gpu.device_string, actual_gpu.device_string);
  }
  EXPECT_EQ(driver_vendor, output.gpu.driver_vendor);
  EXPECT_EQ(driver_version, output.gpu.driver_version);
  EXPECT_EQ(driver_date, output.gpu.driver_date);
  EXPECT_EQ(pixel_shader_version, output.pixel_shader_version);
  EXPECT_EQ(vertex_shader_version, output.vertex_shader_version);
  EXPECT_EQ(max_msaa_samples, output.max_msaa_samples);
  EXPECT_EQ(machine_model_name, output.machine_model_name);
  EXPECT_EQ(machine_model_version, output.machine_model_version);
  EXPECT_EQ(gl_version, output.gl_version);
  EXPECT_EQ(gl_vendor, output.gl_vendor);
  EXPECT_EQ(gl_renderer, output.gl_renderer);
  EXPECT_EQ(gl_extensions, output.gl_extensions);
  EXPECT_EQ(gl_ws_vendor, output.gl_ws_vendor);
  EXPECT_EQ(gl_ws_version, output.gl_ws_version);
  EXPECT_EQ(gl_ws_extensions, output.gl_ws_extensions);
  EXPECT_EQ(gl_reset_notification_strategy,
            output.gl_reset_notification_strategy);
  EXPECT_EQ(software_rendering, output.software_rendering);
  EXPECT_EQ(direct_rendering, output.direct_rendering);
  EXPECT_EQ(sandboxed, output.sandboxed);
  EXPECT_EQ(in_process_gpu, output.in_process_gpu);
  EXPECT_EQ(passthrough_cmd_decoder, output.passthrough_cmd_decoder);
#if defined(OS_WIN)
  EXPECT_EQ(direct_composition_overlays, output.direct_composition_overlays);
  EXPECT_EQ(overlay_capabilities, output.overlay_capabilities);
  EXPECT_EQ(dx_diagnostics.values, output.dx_diagnostics.values);
#endif
  EXPECT_EQ(output.video_decode_accelerator_capabilities.flags,
            video_decode_accelerator_capabilities.flags);
  EXPECT_EQ(
      video_decode_accelerator_capabilities.supported_profiles.size(),
      output.video_decode_accelerator_capabilities.supported_profiles.size());
  for (size_t i = 0;
       i < video_decode_accelerator_capabilities.supported_profiles.size();
       ++i) {
    const gpu::VideoDecodeAcceleratorSupportedProfile& expected =
        video_decode_accelerator_capabilities.supported_profiles[i];
    const gpu::VideoDecodeAcceleratorSupportedProfile& actual =
        output.video_decode_accelerator_capabilities.supported_profiles[i];
    EXPECT_EQ(expected.encrypted_only, actual.encrypted_only);
  }
  EXPECT_EQ(
      output.video_decode_accelerator_capabilities.supported_profiles.size(),
      video_decode_accelerator_capabilities.supported_profiles.size());
  EXPECT_EQ(output.video_encode_accelerator_supported_profiles.size(),
            video_encode_accelerator_supported_profiles.size());
#if defined(USE_X11)
  EXPECT_EQ(system_visual, output.system_visual);
  EXPECT_EQ(rgba_visual, output.rgba_visual);
#endif
}

TEST_F(StructTraitsTest, EmptyGpuInfo) {
  gpu::GPUInfo input;
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::GPUInfo output;
  proxy->EchoGpuInfo(input, &output);
}

TEST_F(StructTraitsTest, Mailbox) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2};
  gpu::Mailbox input;
  input.SetName(mailbox_name);
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::Mailbox output;
  proxy->EchoMailbox(input, &output);
  gpu::Mailbox test_mailbox;
  test_mailbox.SetName(mailbox_name);
  EXPECT_EQ(test_mailbox, output);
}

TEST_F(StructTraitsTest, MailboxHolder) {
  gpu::MailboxHolder input;

  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2};
  gpu::Mailbox mailbox;
  mailbox.SetName(mailbox_name);

  const gpu::CommandBufferNamespace namespace_id = gpu::IN_PROCESS;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdeadL;
  gpu::SyncToken sync_token(namespace_id, command_buffer_id, release_count);
  sync_token.SetVerifyFlush();

  const uint32_t texture_target = 1337;

  input.mailbox = mailbox;
  input.sync_token = sync_token;
  input.texture_target = texture_target;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::MailboxHolder output;
  proxy->EchoMailboxHolder(input, &output);
  EXPECT_EQ(mailbox, output.mailbox);
  EXPECT_EQ(sync_token, output.sync_token);
  EXPECT_EQ(texture_target, output.texture_target);
}

TEST_F(StructTraitsTest, SyncToken) {
  const gpu::CommandBufferNamespace namespace_id = gpu::IN_PROCESS;
  const gpu::CommandBufferId command_buffer_id(
      gpu::CommandBufferId::FromUnsafeValue(0xdeadbeef));
  const uint64_t release_count = 0xdeadbeefdead;
  gpu::SyncToken input(namespace_id, command_buffer_id, release_count);
  input.SetVerifyFlush();
  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::SyncToken output;
  proxy->EchoSyncToken(input, &output);
  EXPECT_EQ(namespace_id, output.namespace_id());
  EXPECT_EQ(command_buffer_id, output.command_buffer_id());
  EXPECT_EQ(release_count, output.release_count());
  EXPECT_TRUE(output.verified_flush());
}

TEST_F(StructTraitsTest, VideoDecodeAcceleratorSupportedProfile) {
  const gpu::VideoCodecProfile profile =
      gpu::VideoCodecProfile::H264PROFILE_MAIN;
  const int32_t max_width = 1920;
  const int32_t max_height = 1080;
  const int32_t min_width = 640;
  const int32_t min_height = 480;
  const gfx::Size max_resolution(max_width, max_height);
  const gfx::Size min_resolution(min_width, min_height);

  gpu::VideoDecodeAcceleratorSupportedProfile input;
  input.profile = profile;
  input.max_resolution = max_resolution;
  input.min_resolution = min_resolution;
  input.encrypted_only = false;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::VideoDecodeAcceleratorSupportedProfile output;
  proxy->EchoVideoDecodeAcceleratorSupportedProfile(input, &output);
  EXPECT_EQ(profile, output.profile);
  EXPECT_EQ(max_resolution, output.max_resolution);
  EXPECT_EQ(min_resolution, output.min_resolution);
  EXPECT_FALSE(output.encrypted_only);
}

TEST_F(StructTraitsTest, VideoDecodeAcceleratorCapabilities) {
  const uint32_t flags = 1234;

  gpu::VideoDecodeAcceleratorCapabilities input;
  input.flags = flags;
  input.supported_profiles.push_back(
      gpu::VideoDecodeAcceleratorSupportedProfile());
  input.supported_profiles.push_back(
      gpu::VideoDecodeAcceleratorSupportedProfile());

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::VideoDecodeAcceleratorCapabilities output;
  proxy->EchoVideoDecodeAcceleratorCapabilities(input, &output);
  EXPECT_EQ(flags, output.flags);
  EXPECT_EQ(input.supported_profiles.size(), output.supported_profiles.size());
}

TEST_F(StructTraitsTest, VideoEncodeAcceleratorSupportedProfile) {
  const gpu::VideoCodecProfile profile =
      gpu::VideoCodecProfile::H264PROFILE_MAIN;
  const gfx::Size max_resolution(1920, 1080);
  const uint32_t max_framerate_numerator = 144;
  const uint32_t max_framerate_denominator = 12;

  gpu::VideoEncodeAcceleratorSupportedProfile input;
  input.profile = profile;
  input.max_resolution = max_resolution;
  input.max_framerate_numerator = max_framerate_numerator;
  input.max_framerate_denominator = max_framerate_denominator;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  gpu::VideoEncodeAcceleratorSupportedProfile output;
  proxy->EchoVideoEncodeAcceleratorSupportedProfile(input, &output);
  EXPECT_EQ(profile, output.profile);
  EXPECT_EQ(max_resolution, output.max_resolution);
  EXPECT_EQ(max_framerate_numerator, output.max_framerate_numerator);
  EXPECT_EQ(max_framerate_denominator, output.max_framerate_denominator);
}

TEST_F(StructTraitsTest, GpuPreferences) {
  GpuPreferences prefs;
  prefs.single_process = true;
  prefs.in_process_gpu = true;
#if defined(OS_WIN)
  const GpuPreferences::VpxDecodeVendors vendor =
      GpuPreferences::VPX_VENDOR_AMD;
  prefs.enable_accelerated_vpx_decode = vendor;
#endif
  prefs.enable_gpu_driver_debug_logging = true;

  mojom::TraitsTestServicePtr proxy = GetTraitsTestProxy();
  GpuPreferences echo;
  proxy->EchoGpuPreferences(prefs, &echo);
  EXPECT_TRUE(echo.single_process);
  EXPECT_TRUE(echo.in_process_gpu);
  EXPECT_TRUE(echo.enable_gpu_driver_debug_logging);
#if defined(OS_WIN)
  EXPECT_EQ(vendor, echo.enable_accelerated_vpx_decode);
#endif
}

TEST_F(StructTraitsTest, GpuFeatureInfo) {
  GpuFeatureInfo input;
  input.status_values[GPU_FEATURE_TYPE_FLASH3D] =
      gpu::kGpuFeatureStatusBlacklisted;
  input.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      gpu::kGpuFeatureStatusUndefined;
  input.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      gpu::kGpuFeatureStatusDisabled;
  input.status_values[GPU_FEATURE_TYPE_OOP_RASTERIZATION] =
      gpu::kGpuFeatureStatusDisabled;

  GpuFeatureInfo output;
  ASSERT_TRUE(mojom::GpuFeatureInfo::Deserialize(
      mojom::GpuFeatureInfo::Serialize(&input), &output));
  EXPECT_TRUE(std::equal(input.status_values,
                         input.status_values + NUMBER_OF_GPU_FEATURE_TYPES,
                         output.status_values));
}

}  // namespace gpu
