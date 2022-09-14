// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/alsa/alsa_util.h"

#include <stddef.h>

#include <functional>
#include <memory>

#include "base/logging.h"
#include "base/time/time.h"
#include "media/audio/alsa/alsa_wrapper.h"

namespace alsa_util {

namespace {

// Set hardware parameters of PCM. It does the same thing as the corresponding
// part in snd_pcm_set_params() (https://www.alsa-project.org, source code:
// https://github.com/tiwai/alsa-lib/blob/master/src/pcm/pcm.c#L8459), except
// that it configures buffer size and period size both to closest available
// values instead of forcing the buffer size be 4 times of the period size.
int ConfigureHwParams(media::AlsaWrapper* wrapper,
                      snd_pcm_t* handle,
                      snd_pcm_format_t format,
                      snd_pcm_access_t access,
                      unsigned int channels,
                      unsigned int sample_rate,
                      int soft_resample,
                      snd_pcm_uframes_t frames_per_buffer,
                      snd_pcm_uframes_t frames_per_period) {
  int error = 0;

  snd_pcm_hw_params_t* hw_params = nullptr;
  error = wrapper->PcmHwParamsMalloc(&hw_params);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsMalloc: " << wrapper->StrError(error);
    return error;
  }
  // |snd_pcm_hw_params_t| is not exposed and requires memory allocation through
  // ALSA API. Therefore, use a smart pointer to pointer to insure freeing
  // memory when the function returns.
  std::unique_ptr<snd_pcm_hw_params_t*,
                  std::function<void(snd_pcm_hw_params_t**)>>
      params_holder(&hw_params, [wrapper](snd_pcm_hw_params_t** params) {
        wrapper->PcmHwParamsFree(*params);
      });

  error = wrapper->PcmHwParamsAny(handle, hw_params);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsAny: " << wrapper->StrError(error);
    return error;
  }

  error = wrapper->PcmHwParamsSetRateResample(handle, hw_params, soft_resample);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetRateResample: " << wrapper->StrError(error);
    return error;
  }

  error = wrapper->PcmHwParamsSetAccess(handle, hw_params, access);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetAccess: " << wrapper->StrError(error);
    return error;
  }

  error = wrapper->PcmHwParamsSetFormat(handle, hw_params, format);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetFormat: " << wrapper->StrError(error);
    return error;
  }

  error = wrapper->PcmHwParamsSetChannels(handle, hw_params, channels);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetChannels: " << wrapper->StrError(error);
    return error;
  }

  unsigned int rate = sample_rate;
  error = wrapper->PcmHwParamsSetRateNear(handle, hw_params, &rate, nullptr);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetRateNear: " << wrapper->StrError(error);
    return error;
  }
  if (rate != sample_rate) {
    LOG(ERROR) << "Rate doesn't match, required: " << sample_rate
               << "Hz, but get: " << rate << "Hz.";
    return -EINVAL;
  }

  error = wrapper->PcmHwParamsSetBufferSizeNear(handle, hw_params,
                                                &frames_per_buffer);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetBufferSizeNear: " << wrapper->StrError(error);
    return error;
  }

  int direction = 0;
  error = wrapper->PcmHwParamsSetPeriodSizeNear(handle, hw_params,
                                                &frames_per_period, &direction);
  if (error < 0) {
    LOG(ERROR) << "PcmHwParamsSetPeriodSizeNear: " << wrapper->StrError(error);
    return error;
  }

  if (frames_per_period > frames_per_buffer / 2) {
    LOG(ERROR) << "Period size (" << frames_per_period
               << ") is too big; buffer size = " << frames_per_buffer;
    return -EINVAL;
  }

  error = wrapper->PcmHwParams(handle, hw_params);
  if (error < 0)
    LOG(ERROR) << "PcmHwParams: " << wrapper->StrError(error);

  return error;
}

// Set software parameters of PCM. It does the same thing as the corresponding
// part in snd_pcm_set_params()
// (https://github.com/tiwai/alsa-lib/blob/master/src/pcm/pcm.c#L8603).
int ConfigureSwParams(media::AlsaWrapper* wrapper,
                      snd_pcm_t* handle,
                      snd_pcm_uframes_t frames_per_buffer,
                      snd_pcm_uframes_t frames_per_period) {
  int error = 0;

  snd_pcm_sw_params_t* sw_params = nullptr;
  error = wrapper->PcmSwParamsMalloc(&sw_params);
  if (error < 0) {
    LOG(ERROR) << "PcmSwParamsMalloc: " << wrapper->StrError(error);
    return error;
  }
  // |snd_pcm_sw_params_t| is not exposed and thus use a smart pointer to
  // pointer to insure freeing memory when the function returns.
  std::unique_ptr<snd_pcm_sw_params_t*,
                  std::function<void(snd_pcm_sw_params_t**)>>
      params_holder(&sw_params, [wrapper](snd_pcm_sw_params_t** params) {
        wrapper->PcmSwParamsFree(*params);
      });

  error = wrapper->PcmSwParamsCurrent(handle, sw_params);
  if (error < 0) {
    LOG(ERROR) << "PcmSwParamsCurrent: " << wrapper->StrError(error);
    return error;
  }

  // For playback, start the transfer when the buffer is almost full.
  int start_threshold =
      (frames_per_buffer / frames_per_period) * frames_per_period;
  error =
      wrapper->PcmSwParamsSetStartThreshold(handle, sw_params, start_threshold);
  if (error < 0) {
    LOG(ERROR) << "PcmSwParamsSetStartThreshold: " << wrapper->StrError(error);
    return error;
  }

  // For capture, wake capture thread as soon as possible (1 period).
  error = wrapper->PcmSwParamsSetAvailMin(handle, sw_params, frames_per_period);
  if (error < 0) {
    LOG(ERROR) << "PcmSwParamsSetAvailMin: " << wrapper->StrError(error);
    return error;
  }

  error = wrapper->PcmSwParams(handle, sw_params);
  if (error < 0)
    LOG(ERROR) << "PcmSwParams: " << wrapper->StrError(error);

  return error;
}

int SetParams(media::AlsaWrapper* wrapper,
              snd_pcm_t* handle,
              snd_pcm_format_t format,
              unsigned int channels,
              unsigned int rate,
              unsigned int frames_per_buffer,
              unsigned int frames_per_period) {
  int error = ConfigureHwParams(
      wrapper, handle, format, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate,
      1 /* Enable resampling */, frames_per_buffer, frames_per_period);
  if (error == 0) {
    error = ConfigureSwParams(wrapper, handle, frames_per_buffer,
                              frames_per_period);
  }
  return error;
}

}  // namespace

static snd_pcm_t* OpenDevice(media::AlsaWrapper* wrapper,
                             const char* device_name,
                             snd_pcm_stream_t type,
                             int channels,
                             int sample_rate,
                             snd_pcm_format_t pcm_format,
                             int buffer_us,
                             int period_us = 0) {
  snd_pcm_t* handle = NULL;
  int error = wrapper->PcmOpen(&handle, device_name, type, SND_PCM_NONBLOCK);
  if (error < 0) {
    LOG(ERROR) << "PcmOpen: " << device_name << "," << wrapper->StrError(error);
    return NULL;
  }

  error =
      wrapper->PcmSetParams(handle, pcm_format, SND_PCM_ACCESS_RW_INTERLEAVED,
                            channels, sample_rate, 1, buffer_us);
  if (error < 0) {
    LOG(WARNING) << "PcmSetParams: " << device_name << ", "
                 << wrapper->StrError(error);
    // Default parameter setting function failed, try again with the customized
    // one if |period_us| is set, which is the case for capture but not for
    // playback.
    if (period_us > 0) {
      const unsigned int frames_per_buffer = static_cast<unsigned int>(
          static_cast<int64_t>(buffer_us) * sample_rate /
          base::Time::kMicrosecondsPerSecond);
      const unsigned int frames_per_period = static_cast<unsigned int>(
          static_cast<int64_t>(period_us) * sample_rate /
          base::Time::kMicrosecondsPerSecond);
      LOG(WARNING) << "SetParams: " << device_name
                   << " - Format: " << pcm_format << " Channels: " << channels
                   << " Sample rate: " << sample_rate
                   << " Buffer size: " << frames_per_buffer
                   << " Period size: " << frames_per_period;
      error = SetParams(wrapper, handle, pcm_format, channels, sample_rate,
                        frames_per_buffer, frames_per_period);
    }
  }
  if (error < 0) {
    if (alsa_util::CloseDevice(wrapper, handle) < 0) {
      // TODO(ajwong): Retry on certain errors?
      LOG(WARNING) << "Unable to close audio device. Leaking handle.";
    }
    return NULL;
  }

  return handle;
}

static std::string DeviceNameToControlName(const std::string& device_name) {
  const char kMixerPrefix[] = "hw";
  std::string control_name;
  size_t pos1 = device_name.find(':');
  if (pos1 == std::string::npos) {
    control_name = device_name;
  } else {
    // Examples:
    // deviceName: "front:CARD=Intel,DEV=0", controlName: "hw:CARD=Intel".
    // deviceName: "default:CARD=Intel", controlName: "CARD=Intel".
    size_t pos2 = device_name.find(',');
    control_name = (pos2 == std::string::npos)
                       ? device_name.substr(pos1 + 1)
                       : kMixerPrefix + device_name.substr(pos1, pos2 - pos1);
  }

  return control_name;
}

int CloseDevice(media::AlsaWrapper* wrapper, snd_pcm_t* handle) {
  std::string device_name = wrapper->PcmName(handle);
  int error = wrapper->PcmClose(handle);
  if (error < 0) {
    LOG(ERROR) << "PcmClose: " << device_name << ", "
               << wrapper->StrError(error);
  }

  return error;
}

snd_pcm_t* OpenCaptureDevice(media::AlsaWrapper* wrapper,
                             const char* device_name,
                             int channels,
                             int sample_rate,
                             snd_pcm_format_t pcm_format,
                             int buffer_us,
                             int period_us) {
  return OpenDevice(wrapper, device_name, SND_PCM_STREAM_CAPTURE, channels,
                    sample_rate, pcm_format, buffer_us, period_us);
}

snd_pcm_t* OpenPlaybackDevice(media::AlsaWrapper* wrapper,
                              const char* device_name,
                              int channels,
                              int sample_rate,
                              snd_pcm_format_t pcm_format,
                              int buffer_us) {
  return OpenDevice(wrapper, device_name, SND_PCM_STREAM_PLAYBACK, channels,
                    sample_rate, pcm_format, buffer_us);
}

snd_mixer_t* OpenMixer(media::AlsaWrapper* wrapper,
                       const std::string& device_name) {
  snd_mixer_t* mixer = NULL;

  int error = wrapper->MixerOpen(&mixer, 0);
  if (error < 0) {
    LOG(ERROR) << "MixerOpen: " << device_name << ", "
               << wrapper->StrError(error);
    return NULL;
  }

  std::string control_name = DeviceNameToControlName(device_name);
  error = wrapper->MixerAttach(mixer, control_name.c_str());
  if (error < 0) {
    LOG(ERROR) << "MixerAttach, " << control_name << ", "
               << wrapper->StrError(error);
    alsa_util::CloseMixer(wrapper, mixer, device_name);
    return NULL;
  }

  error = wrapper->MixerElementRegister(mixer, NULL, NULL);
  if (error < 0) {
    LOG(ERROR) << "MixerElementRegister: " << control_name << ", "
               << wrapper->StrError(error);
    alsa_util::CloseMixer(wrapper, mixer, device_name);
    return NULL;
  }

  return mixer;
}

void CloseMixer(media::AlsaWrapper* wrapper, snd_mixer_t* mixer,
                const std::string& device_name) {
  if (!mixer)
    return;

  wrapper->MixerFree(mixer);

  int error = 0;
  if (!device_name.empty()) {
    std::string control_name = DeviceNameToControlName(device_name);
    error = wrapper->MixerDetach(mixer, control_name.c_str());
    if (error < 0) {
      LOG(WARNING) << "MixerDetach: " << control_name << ", "
                   << wrapper->StrError(error);
    }
  }

  error = wrapper->MixerClose(mixer);
  if (error < 0) {
    LOG(WARNING) << "MixerClose: " << wrapper->StrError(error);
  }
}

snd_mixer_elem_t* LoadCaptureMixerElement(media::AlsaWrapper* wrapper,
                                          snd_mixer_t* mixer) {
  if (!mixer)
    return NULL;

  int error = wrapper->MixerLoad(mixer);
  if (error < 0) {
    LOG(ERROR) << "MixerLoad: " << wrapper->StrError(error);
    return NULL;
  }

  snd_mixer_elem_t* elem = NULL;
  snd_mixer_elem_t* mic_elem = NULL;
  const char kCaptureElemName[] = "Capture";
  const char kMicElemName[] = "Mic";
  for (elem = wrapper->MixerFirstElem(mixer);
       elem;
       elem = wrapper->MixerNextElem(elem)) {
    if (wrapper->MixerSelemIsActive(elem)) {
      const char* elem_name = wrapper->MixerSelemName(elem);
      if (strcmp(elem_name, kCaptureElemName) == 0)
        return elem;
      else if (strcmp(elem_name, kMicElemName) == 0)
        mic_elem = elem;
    }
  }

  // Did not find any Capture handle, use the Mic handle.
  return mic_elem;
}

}  // namespace alsa_util
