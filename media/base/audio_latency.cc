// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/audio_latency.h"

#include <stdint.h>

#include <algorithm>
#include <cmath>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/base/limits.h"
#include "media/media_buildflags.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "media/base/mac/audio_latency_mac.h"
#endif

#if BUILDFLAG(IS_FUCHSIA)
#include "base/fuchsia/scheduler.h"
#endif

namespace media {

namespace {

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_FUCHSIA)
// Taken from "Bit Twiddling Hacks"
// http://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
uint32_t RoundUpToPowerOfTwo(uint32_t v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;
  return v;
}
#endif

#if BUILDFLAG(IS_ANDROID)
// WebAudio renderer's quantum size (frames per callback) that is used for
// calculating the "interactive" buffer size.
// TODO(crbug.com/40637820): This number needs to be passed down from Blink when
// user-selectable render quantum size is implemented.
const int kWebAudioRenderQuantumSize = 128;

// From media/renderers/paint_canvas_video_renderer.cc. To calculate the optimum
// buffer size for Pixel 3/4/5 devices, which has a HW buffer size of 96 frames.
int GCD(int a, int b) {
  return a == 0 ? b : GCD(b % a, a);
}

int LCM(int a, int b) {
  return a / GCD(a, b) * b;
}
#endif

}  // namespace

// static
bool AudioLatency::IsResamplingPassthroughSupported(Type type) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return true;
#elif BUILDFLAG(IS_FUCHSIA)
  return true;
#elif BUILDFLAG(IS_ANDROID)
  // Only N MR1+ has support for OpenSLES performance modes which allow for
  // power efficient playback. Per the Android audio team, we shouldn't waste
  // cycles on resampling when using the playback mode. See OpenSLESOutputStream
  // for additional implementation details.
  return type == Type::kPlayback &&
         base::android::BuildInfo::GetInstance()->sdk_int() >=
             base::android::SDK_VERSION_NOUGAT_MR1;
#else
  return false;
#endif
}

// static
int AudioLatency::GetHighLatencyBufferSize(int sample_rate,
                                           int preferred_buffer_size) {
#if BUILDFLAG(USE_CRAS)
  // Use 80ms rounded to a power of 2.
  const double eighty_ms_size = 8.0 * sample_rate / 100;
  const int high_latency_buffer_size = RoundUpToPowerOfTwo(eighty_ms_size);
#elif BUILDFLAG(IS_FUCHSIA)
  // Use 80ms buffers. Doesn't need to be aligned to power of 2, but it should
  // be a multiple of the scheduling period used for audio threads.
  constexpr base::TimeDelta period = base::Milliseconds(80);
  static_assert(static_cast<int>(period / base::kAudioSchedulingPeriod) ==
                period / base::kAudioSchedulingPeriod);
  const int high_latency_buffer_size = period.InMilliseconds() * sample_rate /
                                       base::Time::kMillisecondsPerSecond;
#elif BUILDFLAG(IS_WIN)
  const double twenty_ms_size = 2.0 * sample_rate / 100;
  preferred_buffer_size = std::max(preferred_buffer_size, 1);

  // Windows doesn't use power of two buffer sizes, so we should always round up
  // to the nearest multiple of the output buffer size.
  const int high_latency_buffer_size =
      std::ceil(twenty_ms_size / preferred_buffer_size) * preferred_buffer_size;
#else
  // On other platforms use the nearest higher power of two buffer size.  For a
  // given sample rate, this works out to:
  //
  //     <= 3200   : 64
  //     <= 6400   : 128
  //     <= 12800  : 256
  //     <= 25600  : 512
  //     <= 51200  : 1024
  //     <= 102400 : 2048
  //     <= 204800 : 4096
  //
  // On Linux, the minimum hardware buffer size is 512, so the lower calculated
  // values are unused.  OSX may have a value as low as 128.
  const double twenty_ms_size = 2.0 * sample_rate / 100;
  const int high_latency_buffer_size = RoundUpToPowerOfTwo(twenty_ms_size);
#endif

  return std::max(preferred_buffer_size, high_latency_buffer_size);
}

// static
int AudioLatency::GetRtcBufferSize(int sample_rate, int hardware_buffer_size) {
  // Use native hardware buffer size as default. On Windows, we strive to open
  // up using this native hardware buffer size to achieve best
  // possible performance and to ensure that no FIFO is needed on the browser
  // side to match the client request. That is why there is no #if case for
  // Windows below.
  int frames_per_buffer = hardware_buffer_size;

  // No |hardware_buffer_size| is specified, fall back to 10 ms buffer size.
  if (!frames_per_buffer) {
    frames_per_buffer = sample_rate / 100;
    DVLOG(1) << "Using 10 ms sink output buffer size: " << frames_per_buffer;
    return frames_per_buffer;
  }

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_APPLE) || \
    BUILDFLAG(IS_FUCHSIA)
  // On Linux, MacOS and Fuchsia, the low level IO implementations on the
  // browser side supports all buffer size the clients want. We use the native
  // peer connection buffer size (10ms) to achieve best possible performance.
  frames_per_buffer = sample_rate / 100;
#elif BUILDFLAG(IS_ANDROID)
  // TODO(olka/henrika): This settings are very old, need to be revisited.
  int frames_per_10ms = sample_rate / 100;
  if (frames_per_buffer < 2 * frames_per_10ms) {
    // Examples of low-latency frame sizes and the resulting |buffer_size|:
    //  Nexus 7     : 240 audio frames => 2*480 = 960
    //  Nexus 10    : 256              => 2*441 = 882
    //  Galaxy Nexus: 144              => 2*441 = 882
    frames_per_buffer = 2 * frames_per_10ms;
    DVLOG(1) << "Low-latency output detected on Android";
  }
#endif

  DVLOG(1) << "Using sink output buffer size: " << frames_per_buffer;
  return frames_per_buffer;
}

// static
int AudioLatency::GetInteractiveBufferSize(int hardware_buffer_size) {
  CHECK_GT(hardware_buffer_size, 0);

#if BUILDFLAG(IS_ANDROID)
  // Always log this because it's relatively hard to get this
  // information out.
  LOG(INFO) << "audioHardwareBufferSize = " << hardware_buffer_size;

  if (hardware_buffer_size >= kWebAudioRenderQuantumSize)
    return hardware_buffer_size;

  // HW buffer size is smaller than the Web Audio's render quantum size, so
  // compute LCM to avoid glitches and regulate the workload per callback.
  // (e.g. 96 vs 128 -> 384) Also cap the buffer size to 4 render quanta
  // (512 frames ~= 10ms at 48K) if LCM goes beyond interactive latency range.
  int sensible_buffer_size = std::min(
      LCM(hardware_buffer_size, kWebAudioRenderQuantumSize),
      kWebAudioRenderQuantumSize * 4);

  return sensible_buffer_size;
#else
  return hardware_buffer_size;
#endif  // BUILDFLAG(IS_ANDROID)
}

int AudioLatency::GetExactBufferSize(base::TimeDelta duration,
                                     int sample_rate,
                                     int hardware_buffer_size,
                                     int min_hardware_buffer_size,
                                     int max_hardware_buffer_size,
                                     int max_allowed_buffer_size) {
  DCHECK_NE(0, hardware_buffer_size);
  DCHECK_NE(0, max_allowed_buffer_size);
  DCHECK_GE(hardware_buffer_size, min_hardware_buffer_size);
  DCHECK_GE(max_hardware_buffer_size, min_hardware_buffer_size);
  DCHECK(max_hardware_buffer_size == 0 ||
         hardware_buffer_size <= max_hardware_buffer_size);
  DCHECK_LE(hardware_buffer_size, max_allowed_buffer_size);

  int requested_buffer_size = std::round(duration.InSecondsF() * sample_rate);

  if (min_hardware_buffer_size &&
      requested_buffer_size <= min_hardware_buffer_size) {
    return min_hardware_buffer_size;
  }

  if (requested_buffer_size <= hardware_buffer_size)
    return hardware_buffer_size;

#if BUILDFLAG(IS_WIN)
  // On Windows we allow either exactly the minimum buffer size (using
  // IAudioClient3) or multiples of the default buffer size using the previous
  // IAudioClient API.
  const int multiplier = hardware_buffer_size;
#else
  const int multiplier = min_hardware_buffer_size > 0 ? min_hardware_buffer_size
                                                      : hardware_buffer_size;
#endif

  int buffer_size =
      std::ceil(requested_buffer_size / static_cast<double>(multiplier)) *
      multiplier;

  // If the user is requesting a buffer size >= max_hardware_buffer_size then we
  // want the hardware to run at this max and then only return sizes that are
  // multiples of this here so that we don't end up with Web Audio running with
  // a period that's misaligned with the hardware one.
  if (max_hardware_buffer_size && buffer_size >= max_hardware_buffer_size) {
    buffer_size = std::ceil(requested_buffer_size /
                            static_cast<double>(max_hardware_buffer_size)) *
                  max_hardware_buffer_size;
  }

  const int platform_max_buffer_size =
      (max_hardware_buffer_size &&
       max_hardware_buffer_size <= max_allowed_buffer_size)
          ? (max_allowed_buffer_size / max_hardware_buffer_size) *
                max_hardware_buffer_size
          : (max_allowed_buffer_size / multiplier) * multiplier;

  return std::min(buffer_size, platform_max_buffer_size);
}

// static
// Used for UMA histogram names, do not change the lookup.
const char* AudioLatency::ToString(Type type) {
  switch (type) {
    case Type::kExactMS:
      return "LatencyExactMs";
    case Type::kInteractive:
      return "LatencyInteractive";
    case Type::kRtc:
      return "LatencyRtc";
    case Type::kPlayback:
      return "LatencyPlayback";
    case Type::kUnknown:
      return "LatencyUnknown";
  }
}
}  // namespace media
