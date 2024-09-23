# Do NOT add net/ or ui/base without a great reason, they're huge!
include_rules = [
  "+cc/base/math_util.h",
  "+cc/paint",
  "+components/crash/core/common/crash_key.h",
  "+components/system_media_controls/linux/buildflags",
  "+components/viz/common/resources",
  "+crypto",
  "+device/udev_linux",
  "+gpu",
  "+media/midi/midi_jni_headers",
  "+mojo/public/cpp/bindings/callback_helpers.h",
  "+mojo/public/cpp/system/platform_handle.h",
  "+net/cookies/site_for_cookies.h",
  "+net/storage_access_api/status.h",
  "+services/device/public",
  "+services/viz/public/cpp/gpu/context_provider_command_buffer.h",
  "+skia/ext",
  "+third_party/re2",
  "+third_party/dav1d",
  "+third_party/ffmpeg",
  "+third_party/flac",
  "+third_party/fuzztest",
  "+third_party/libaom",
  "+third_party/libdrm",
  "+third_party/libgav1",
  "+third_party/libvpx",
  "+third_party/libyuv",
  "+third_party/openh264/src/codec/api/wels",
  "+third_party/opus",
  "+third_party/skia",
  "+ui/base/x/x11_user_input_monitor.h",
  "+ui/display",
  "+ui/events",
  "+ui/gfx",
  "+ui/gl",
  "+ui/ozone",
  "+third_party/widevine/cdm/widevine_cdm_common.h",
  "-ipc",
  "-media/webrtc",
]

specific_include_rules = {
  "audio_manager_unittest.cc": [
    "+chromeos/ash/components/dbus"
  ],
  "cras_input_unittest.cc": [
    "+chromeos/ash/components/dbus"
  ],
  "cras_unified_unittest.cc": [
    "+chromeos/ash/components/dbus"
  ],
  "fuchsia_video_decoder_unittest.cc": [
    "+components/viz/test/test_context_support.h",
  ],
  "gpu_memory_buffer_video_frame_pool_unittest.cc": [
    "+components/viz/test/test_context_provider.h",
  ],
  "video_encode_accelerator_adapter_test.cc": [
    "+components/viz/test/test_context_provider.h"
  ],
  "renderable_gpu_memory_buffer_video_frame_pool_unittest.cc": [
    "+components/viz/test/test_context_provider.h",
  ],
  "null_video_sink_unittest.cc": [
    "+components/viz/common/frame_sinks/begin_frame_args.h",
  ],
  "fuchsia_.*_mojom_traits\.h": [
    "+mojo/public/cpp/base/fuchsia/fidl_interface_request_mojom_traits.h",
  ],
}
