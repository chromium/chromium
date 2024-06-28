// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>

#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/ipc/common/gpu_feature_info.mojom.h"
#include "gpu/ipc/common/gpu_feature_info_mojom_traits.h"
#include "gpu/ipc/common/traits_test_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

namespace {

class StructTraitsTest : public testing::Test, public mojom::TraitsTestService {
 public:
  StructTraitsTest() = default;

  StructTraitsTest(const StructTraitsTest&) = delete;
  StructTraitsTest& operator=(const StructTraitsTest&) = delete;

 protected:
  mojo::Remote<mojom::TraitsTestService> GetTraitsTestRemote() {
    mojo::Remote<mojom::TraitsTestService> remote;
    traits_test_receivers_.Add(this, remote.BindNewPipeAndPassReceiver());
    return remote;
  }

 private:
  // TraitsTestService:
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

  base::test::TaskEnvironment task_environment_;
  mojo::ReceiverSet<TraitsTestService> traits_test_receivers_;
};

}  // namespace

TEST_F(StructTraitsTest, GPUDevice) {
  gpu::GPUInfo::GPUDevice input;
  // Using the values from gpu/config/gpu_info_collector_unittest.cc::nvidia_gpu
  const uint32_t vendor_id = 0x10de;
  const uint32_t device_id = 0x0df8;
#if BUILDFLAG(IS_WIN)
  const uint32_t sub_sys_id = 0xc0d8144d;
  const uint32_t revision = 4u;
#endif  // BUILDFLAG(IS_WIN)
  const std::string vendor_string = "vendor_string";
  const std::string device_string = "device_string";

  input.vendor_id = vendor_id;
  input.device_id = device_id;
#if BUILDFLAG(IS_WIN)
  input.sub_sys_id = sub_sys_id;
  input.revision = revision;
#endif  // BUILDFLAG(IS_WIN)
  input.vendor_string = vendor_string;
  input.device_string = device_string;
  input.active = false;
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::GPUInfo::GPUDevice output;
  remote->EchoGpuDevice(input, &output);

  EXPECT_EQ(vendor_id, output.vendor_id);
  EXPECT_EQ(device_id, output.device_id);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(sub_sys_id, output.sub_sys_id);
  EXPECT_EQ(revision, output.revision);
#endif  // BUILDFLAG(IS_WIN)
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
  const gl::GLImplementationParts gl_implementation_parts(
      gl::ANGLEImplementation::kSwiftShader);
  const std::string direct_rendering_version = "DRI1";
  const bool sandboxed = true;
  const bool in_process_gpu = true;
  const bool passthrough_cmd_decoder = true;
#if BUILDFLAG(IS_WIN)
  const bool direct_composition = true;
  const bool supports_overlays = true;
  const OverlaySupport yuy2_overlay_support = OverlaySupport::kScaling;
  const OverlaySupport nv12_overlay_support = OverlaySupport::kNone;
  const OverlaySupport bgra8_overlay_support = OverlaySupport::kDirect;
  const OverlaySupport rgb10a2_overlay_support = OverlaySupport::kScaling;
  const OverlaySupport p010_overlay_support = OverlaySupport::kScaling;
#endif
  const VideoDecodeAcceleratorSupportedProfiles
      video_decode_accelerator_supported_profiles;
  const VideoEncodeAcceleratorSupportedProfiles
      video_encode_accelerator_supported_profiles;
  const bool jpeg_decode_accelerator_supported = true;

  gpu::GPUInfo input;
  input.initialization_time = initialization_time;
  input.optimus = optimus;
  input.amd_switchable = amd_switchable;
  input.gpu = gpu;
  input.secondary_gpus = secondary_gpus;
  input.gpu.driver_vendor = driver_vendor;
  input.gpu.driver_version = driver_version;
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
  input.gl_implementation_parts = gl_implementation_parts;
  input.direct_rendering_version = direct_rendering_version;
  input.sandboxed = sandboxed;
  input.in_process_gpu = in_process_gpu;
  input.passthrough_cmd_decoder = passthrough_cmd_decoder;
#if BUILDFLAG(IS_WIN)
  input.overlay_info.direct_composition = direct_composition;
  input.overlay_info.supports_overlays = supports_overlays;
  input.overlay_info.yuy2_overlay_support = yuy2_overlay_support;
  input.overlay_info.nv12_overlay_support = nv12_overlay_support;
  input.overlay_info.bgra8_overlay_support = bgra8_overlay_support;
  input.overlay_info.rgb10a2_overlay_support = rgb10a2_overlay_support;
  input.overlay_info.p010_overlay_support = p010_overlay_support;
#endif
  input.video_decode_accelerator_supported_profiles =
      video_decode_accelerator_supported_profiles;
  input.video_encode_accelerator_supported_profiles =
      video_encode_accelerator_supported_profiles;
  input.jpeg_decode_accelerator_supported = jpeg_decode_accelerator_supported;

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::GPUInfo output;
  remote->EchoGpuInfo(input, &output);

  EXPECT_EQ(optimus, output.optimus);
  EXPECT_EQ(amd_switchable, output.amd_switchable);
  EXPECT_EQ(gpu.vendor_id, output.gpu.vendor_id);
  EXPECT_EQ(gpu.device_id, output.gpu.device_id);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(gpu.sub_sys_id, output.gpu.sub_sys_id);
  EXPECT_EQ(gpu.revision, output.gpu.revision);
#endif  // BUILDFLAG(IS_WIN)
  EXPECT_EQ(gpu.active, output.gpu.active);
  EXPECT_EQ(gpu.vendor_string, output.gpu.vendor_string);
  EXPECT_EQ(gpu.device_string, output.gpu.device_string);
  EXPECT_EQ(secondary_gpus.size(), output.secondary_gpus.size());
  for (size_t i = 0; i < secondary_gpus.size(); ++i) {
    const gpu::GPUInfo::GPUDevice& expected_gpu = secondary_gpus[i];
    const gpu::GPUInfo::GPUDevice& actual_gpu = output.secondary_gpus[i];
    EXPECT_EQ(expected_gpu.vendor_id, actual_gpu.vendor_id);
    EXPECT_EQ(expected_gpu.device_id, actual_gpu.device_id);
#if BUILDFLAG(IS_WIN)
    EXPECT_EQ(expected_gpu.sub_sys_id, actual_gpu.sub_sys_id);
    EXPECT_EQ(expected_gpu.revision, actual_gpu.revision);
#endif  // BUILDFLAG(IS_WIN)
    EXPECT_EQ(expected_gpu.active, actual_gpu.active);
    EXPECT_EQ(expected_gpu.vendor_string, actual_gpu.vendor_string);
    EXPECT_EQ(expected_gpu.device_string, actual_gpu.device_string);
  }
  EXPECT_EQ(driver_vendor, output.gpu.driver_vendor);
  EXPECT_EQ(driver_version, output.gpu.driver_version);
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
  EXPECT_EQ(gl_implementation_parts, output.gl_implementation_parts);
  EXPECT_EQ(direct_rendering_version, output.direct_rendering_version);
  EXPECT_EQ(sandboxed, output.sandboxed);
  EXPECT_EQ(in_process_gpu, output.in_process_gpu);
  EXPECT_EQ(passthrough_cmd_decoder, output.passthrough_cmd_decoder);
#if BUILDFLAG(IS_WIN)
  EXPECT_EQ(direct_composition, output.overlay_info.direct_composition);
  EXPECT_EQ(supports_overlays, output.overlay_info.supports_overlays);
  EXPECT_EQ(yuy2_overlay_support, output.overlay_info.yuy2_overlay_support);
  EXPECT_EQ(nv12_overlay_support, output.overlay_info.nv12_overlay_support);
  EXPECT_EQ(bgra8_overlay_support, output.overlay_info.bgra8_overlay_support);
  EXPECT_EQ(rgb10a2_overlay_support,
            output.overlay_info.rgb10a2_overlay_support);
  EXPECT_EQ(p010_overlay_support, output.overlay_info.p010_overlay_support);
#endif
  for (size_t i = 0; i < video_decode_accelerator_supported_profiles.size();
       i++) {
    const gpu::VideoDecodeAcceleratorSupportedProfile& expected =
        video_decode_accelerator_supported_profiles[i];
    const gpu::VideoDecodeAcceleratorSupportedProfile& actual =
        output.video_decode_accelerator_supported_profiles[i];
    EXPECT_EQ(expected.encrypted_only, actual.encrypted_only);
  }
  EXPECT_EQ(output.video_decode_accelerator_supported_profiles.size(),
            video_decode_accelerator_supported_profiles.size());
  EXPECT_EQ(output.video_encode_accelerator_supported_profiles.size(),
            video_encode_accelerator_supported_profiles.size());
}

TEST_F(StructTraitsTest, EmptyGpuInfo) {
  gpu::GPUInfo input;
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::GPUInfo output;
  remote->EchoGpuInfo(input, &output);
}

TEST_F(StructTraitsTest, Mailbox) {
  const int8_t mailbox_name[GL_MAILBOX_SIZE_CHROMIUM] = {
      0, 9, 8, 7, 6, 5, 4, 3, 2, 1, 9, 7, 5, 3, 1, 2};
  gpu::Mailbox input;
  input.SetName(mailbox_name);
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::Mailbox output;
  remote->EchoMailbox(input, &output);
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

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::MailboxHolder output;
  remote->EchoMailboxHolder(input, &output);
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
  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::SyncToken output;
  remote->EchoSyncToken(input, &output);
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

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::VideoDecodeAcceleratorSupportedProfile output;
  remote->EchoVideoDecodeAcceleratorSupportedProfile(input, &output);
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

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::VideoDecodeAcceleratorCapabilities output;
  remote->EchoVideoDecodeAcceleratorCapabilities(input, &output);
  EXPECT_EQ(flags, output.flags);
  EXPECT_EQ(input.supported_profiles.size(), output.supported_profiles.size());
}

TEST_F(StructTraitsTest, VideoEncodeAcceleratorSupportedProfile) {
  const gpu::VideoCodecProfile profile = VideoCodecProfile::H264PROFILE_MAIN;
  const gfx::Size min_resolution(320, 180);
  const gfx::Size max_resolution(1920, 1080);
  const uint32_t max_framerate_numerator = 144;
  const uint32_t max_framerate_denominator = 12;

  gpu::VideoEncodeAcceleratorSupportedProfile input;
  input.profile = profile;
  input.min_resolution = min_resolution;
  input.max_resolution = max_resolution;
  input.max_framerate_numerator = max_framerate_numerator;
  input.max_framerate_denominator = max_framerate_denominator;

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  gpu::VideoEncodeAcceleratorSupportedProfile output;
  remote->EchoVideoEncodeAcceleratorSupportedProfile(input, &output);
  EXPECT_EQ(profile, output.profile);
  EXPECT_EQ(min_resolution, output.min_resolution);
  EXPECT_EQ(max_resolution, output.max_resolution);
  EXPECT_EQ(max_framerate_numerator, output.max_framerate_numerator);
  EXPECT_EQ(max_framerate_denominator, output.max_framerate_denominator);
}

TEST_F(StructTraitsTest, GpuPreferences) {
  GpuPreferences prefs;
  prefs.gpu_startup_dialog = true;
  prefs.disable_gpu_watchdog = true;
  prefs.enable_gpu_driver_debug_logging = true;

  mojo::Remote<mojom::TraitsTestService> remote = GetTraitsTestRemote();
  GpuPreferences echo;
  remote->EchoGpuPreferences(prefs, &echo);
  EXPECT_TRUE(echo.gpu_startup_dialog);
  EXPECT_TRUE(echo.disable_gpu_watchdog);
  EXPECT_TRUE(echo.enable_gpu_driver_debug_logging);
}

TEST_F(StructTraitsTest, GpuFeatureInfo) {
  GpuFeatureInfo input;
  input.status_values[GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS] =
      gpu::kGpuFeatureStatusBlocklisted;
  input.status_values[GPU_FEATURE_TYPE_ACCELERATED_WEBGL] =
      gpu::kGpuFeatureStatusUndefined;
  input.status_values[GPU_FEATURE_TYPE_GPU_TILE_RASTERIZATION] =
      gpu::kGpuFeatureStatusDisabled;

  GpuFeatureInfo output;
  ASSERT_TRUE(mojom::GpuFeatureInfo::Deserialize(
      mojom::GpuFeatureInfo::Serialize(&input), &output));
  EXPECT_EQ(input.status_values, output.status_values);
}

}  // namespace gpu
