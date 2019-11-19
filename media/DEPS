# Do NOT add net/ or ui/base without a great reason, they're huge!
include_rules = [
  "+cc/base/math_util.h",
  "+cc/paint",
  "+chromeos/audio",
  "+components/system_media_controls/linux/buildflags",
  "+crypto",
  "+device/udev_linux",
  "+gpu",
  "+media/midi/midi_jni_headers",
  "+mojo/public/cpp/bindings/callback_helpers.h",
  "+mojo/public/cpp/system/platform_handle.h",
  "+services/device/public",
  "+services/viz/public/cpp/gpu/context_provider_command_buffer.h",
  "+third_party/dav1d",
  "+third_party/ffmpeg",
  "+third_party/libaom",
  "+third_party/libvpx",
  "+third_party/libyuv",
  "+third_party/opus",
  "+third_party/skia",
  "+ui/display",
  "+ui/events",
  "+ui/gfx",
  "+ui/gl",
  "+ui/ozone",
  "+third_party/widevine/cdm/widevine_cdm_common.h",
  "-ipc",
  "-media/blink",
  "-media/webrtc",
]

specific_include_rules = {
  "audio_manager_unittest.cc": [
    "+chromeos/dbus"
  ],
  "cras_input_unittest.cc": [
    "+chromeos/dbus"
  ],
  "cras_unified_unittest.cc": [
    "+chromeos/dbus"
  ],
  "fuchsia_video_decoder_unittest.cc": [
    "+components/viz/test/test_context_support.h",
  ],
  "gpu_memory_buffer_video_frame_pool_unittest.cc": [
    "+components/viz/test/test_context_provider.h",
  ],
  "null_video_sink_unittest.cc": [
    "+components/viz/common/frame_sinks/begin_frame_args.h",
  ],
}
