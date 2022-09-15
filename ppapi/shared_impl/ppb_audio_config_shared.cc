// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/shared_impl/ppb_audio_config_shared.h"

#include <algorithm>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_instance_api.h"

namespace ppapi {

// Rounds up requested_size to the nearest multiple of minimum_size.
static uint32_t CalculateMultipleOfSampleFrameCount(uint32_t minimum_size,
                                                    uint32_t requested_size) {
  const uint32_t multiple = (requested_size + minimum_size - 1) / minimum_size;
  return std::min(minimum_size * multiple,
                  static_cast<uint32_t>(PP_AUDIOMAXSAMPLEFRAMECOUNT));
}

PPB_AudioConfig_Shared::PPB_AudioConfig_Shared(ResourceObjectType type,
                                               PP_Instance instance)
    : Resource(type, instance),
      sample_rate_(PP_AUDIOSAMPLERATE_NONE),
      sample_frame_count_(0) {}

PPB_AudioConfig_Shared::~PPB_AudioConfig_Shared() {}

PP_Resource PPB_AudioConfig_Shared::Create(ResourceObjectType type,
                                           PP_Instance instance,
                                           PP_AudioSampleRate sample_rate,
                                           uint32_t sample_frame_count) {
  scoped_refptr<PPB_AudioConfig_Shared> object(
      new PPB_AudioConfig_Shared(type, instance));
  if (!object->Init(sample_rate, sample_frame_count))
    return 0;
  return object->GetReference();
}

// static
uint32_t PPB_AudioConfig_Shared::RecommendSampleFrameCount_1_0(
    PP_AudioSampleRate sample_rate,
    uint32_t requested_sample_frame_count) {
  // Version 1.0: Don't actually query to get a value from the
  // hardware; instead return the input for in-range values.
  if (requested_sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return PP_AUDIOMINSAMPLEFRAMECOUNT;
  if (requested_sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT)
    return PP_AUDIOMAXSAMPLEFRAMECOUNT;
  return requested_sample_frame_count;
}

// static
uint32_t PPB_AudioConfig_Shared::RecommendSampleFrameCount_1_1(
    PP_Instance instance,
    PP_AudioSampleRate sample_rate,
    uint32_t sample_frame_count) {
  // Version 1.1: Query the back-end hardware for sample rate and buffer size,
  // and recommend a best fit based on request.
  thunk::EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return 0;

  // Get the hardware config.
  PP_AudioSampleRate hardware_sample_rate = static_cast<PP_AudioSampleRate>(
      enter.functions()->GetAudioHardwareOutputSampleRate(instance));
  uint32_t hardware_sample_frame_count =
      enter.functions()->GetAudioHardwareOutputBufferSize(instance);
  if (sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    sample_frame_count = PP_AUDIOMINSAMPLEFRAMECOUNT;

  // If hardware information isn't available we're connected to a fake audio
  // output stream on the browser side, so we can use whatever sample count the
  // client wants.
  if (!hardware_sample_frame_count || !hardware_sample_rate)
    return sample_frame_count;

  // Note: All the values below were determined through experimentation to
  // minimize jitter and back-to-back callbacks from the browser.  Please take
  // care when modifying these values as they impact a large number of users.
  // TODO(dalecurtis): Land jitter test and add documentation for updating this.

  // Should track the value reported by XP and ALSA backends.
  const uint32_t kHighLatencySampleFrameCount = 2048;
#if BUILDFLAG(IS_CHROMEOS_ASH) && defined(ARCH_CPU_ARM_FAMILY)
  // TODO(ihf): Remove this once ARM Chromebooks support low latency audio. For
  // now we classify them as high latency. See crbug.com/289770. Note that
  // Adobe Flash is affected but not HTML5, WebRTC and WebAudio (they are using
  // real time threads).
  const bool kHighLatencyDevice = true;
#else
  const bool kHighLatencyDevice = false;
#endif

  // If client is using same sample rate as audio hardware, then recommend a
  // multiple of the audio hardware's sample frame count.
  if (!kHighLatencyDevice && hardware_sample_rate == sample_rate) {
    return CalculateMultipleOfSampleFrameCount(hardware_sample_frame_count,
                                               sample_frame_count);
  }

  // If the hardware requires a high latency buffer or we're at a low sample
  // rate w/ a buffer that's larger than 10ms, choose the nearest multiple of
  // the high latency sample frame count.  An example of too low and too large
  // is 16kHz and a sample frame count greater than 160 frames.
  if (kHighLatencyDevice ||
      hardware_sample_frame_count >= kHighLatencySampleFrameCount ||
      (hardware_sample_rate < 44100 &&
       hardware_sample_frame_count > hardware_sample_rate / 100u)) {
    return CalculateMultipleOfSampleFrameCount(
        sample_frame_count,
        std::max(kHighLatencySampleFrameCount, hardware_sample_frame_count));
  }

  // All low latency clients should be able to handle a 512 frame buffer with
  // resampling from 44.1kHz and 48kHz to higher sample rates.
  // TODO(dalecurtis): We may need to investigate making the callback thread
  // high priority to handle buffers at the absolute minimum w/o glitching.
  const uint32_t kLowLatencySampleFrameCount = 512;

  // Special case for 48kHz -> 44.1kHz and buffer sizes greater than 10ms.  In
  // testing most buffer sizes > 10ms led to glitching, so we choose a size we
  // know won't cause jitter.
  int min_sample_frame_count = kLowLatencySampleFrameCount;
  if (hardware_sample_rate == 44100 && sample_rate == 48000 &&
      hardware_sample_frame_count > hardware_sample_rate / 100u) {
    min_sample_frame_count =
        std::max(2 * kLowLatencySampleFrameCount, hardware_sample_frame_count);
  }

  return CalculateMultipleOfSampleFrameCount(min_sample_frame_count,
                                             sample_frame_count);
}

// static
PP_AudioSampleRate PPB_AudioConfig_Shared::RecommendSampleRate(
    PP_Instance instance) {
  thunk::EnterInstanceNoLock enter(instance);
  if (enter.failed())
    return PP_AUDIOSAMPLERATE_NONE;
  PP_AudioSampleRate hardware_sample_rate = static_cast<PP_AudioSampleRate>(
      enter.functions()->GetAudioHardwareOutputSampleRate(instance));
  return hardware_sample_rate;
}

thunk::PPB_AudioConfig_API* PPB_AudioConfig_Shared::AsPPB_AudioConfig_API() {
  return this;
}

PP_AudioSampleRate PPB_AudioConfig_Shared::GetSampleRate() {
  return sample_rate_;
}

uint32_t PPB_AudioConfig_Shared::GetSampleFrameCount() {
  return sample_frame_count_;
}

bool PPB_AudioConfig_Shared::Init(PP_AudioSampleRate sample_rate,
                                  uint32_t sample_frame_count) {
  // TODO(brettw): Currently we don't actually check what the hardware
  // supports, so just allow sample rates of the "guaranteed working" ones.
  // TODO(dalecurtis): If sample rates are added RecommendSampleFrameCount_1_1()
  // must be updated to account for the new rates.
  if (sample_rate != PP_AUDIOSAMPLERATE_44100 &&
      sample_rate != PP_AUDIOSAMPLERATE_48000)
    return false;

  // TODO(brettw): Currently we don't actually query to get a value from the
  // hardware, so just validate the range.
  if (sample_frame_count > PP_AUDIOMAXSAMPLEFRAMECOUNT ||
      sample_frame_count < PP_AUDIOMINSAMPLEFRAMECOUNT)
    return false;

  sample_rate_ = sample_rate;
  sample_frame_count_ = sample_frame_count;
  return true;
}

}  // namespace ppapi
