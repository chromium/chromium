// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// AlsaWrapper is a simple stateless class that wraps the alsa library commands
// we want to use.  It's purpose is to allow injection of a mock so that the
// higher level code is testable.

#ifndef MEDIA_AUDIO_ALSA_ALSA_WRAPPER_H_
#define MEDIA_AUDIO_ALSA_ALSA_WRAPPER_H_

#include <alsa/asoundlib.h>

#include "base/macros.h"
#include "media/base/media_export.h"

namespace media {

class MEDIA_EXPORT AlsaWrapper {
 public:
  AlsaWrapper();
  virtual ~AlsaWrapper();

  virtual int DeviceNameHint(int card, const char* iface, void*** hints);
  virtual char* DeviceNameGetHint(const void* hint, const char* id);
  virtual int DeviceNameFreeHint(void** hints);
  virtual int CardNext(int* rcard);

  virtual int PcmOpen(snd_pcm_t** handle, const char* name,
                      snd_pcm_stream_t stream, int mode);
  virtual int PcmClose(snd_pcm_t* handle);
  virtual int PcmPrepare(snd_pcm_t* handle);
  virtual int PcmDrain(snd_pcm_t* handle);
  virtual int PcmDrop(snd_pcm_t* handle);
  virtual int PcmDelay(snd_pcm_t* handle, snd_pcm_sframes_t* delay);
  virtual int PcmResume(snd_pcm_t* handle);
  virtual snd_pcm_sframes_t PcmWritei(snd_pcm_t* handle,
                                      const void* buffer,
                                      snd_pcm_uframes_t size);
  virtual snd_pcm_sframes_t PcmReadi(snd_pcm_t* handle,
                                     void* buffer,
                                     snd_pcm_uframes_t size);
  virtual int PcmRecover(snd_pcm_t* handle, int err, int silent);
  virtual int PcmSetParams(snd_pcm_t* handle, snd_pcm_format_t format,
                           snd_pcm_access_t access, unsigned int channels,
                           unsigned int rate, int soft_resample,
                           unsigned int latency);
  virtual int PcmGetParams(snd_pcm_t* handle, snd_pcm_uframes_t* buffer_size,
                           snd_pcm_uframes_t* period_size);
  virtual int PcmHwParamsMalloc(snd_pcm_hw_params_t** hw_params);
  virtual int PcmHwParamsAny(snd_pcm_t* handle, snd_pcm_hw_params_t* hw_params);
  virtual int PcmHwParamsCanResume(snd_pcm_hw_params_t* hw_params);
  virtual int PcmHwParamsSetRateResample(snd_pcm_t* handle,
                                         snd_pcm_hw_params_t* hw_params,
                                         unsigned int value);
  virtual int PcmHwParamsSetRateNear(snd_pcm_t* handle,
                                     snd_pcm_hw_params_t* hw_params,
                                     unsigned int* rate,
                                     int* direction);
  virtual int PcmHwParamsTestFormat(snd_pcm_t* handle,
                                    snd_pcm_hw_params_t* hw_params,
                                    snd_pcm_format_t format);
  virtual int PcmFormatSize(snd_pcm_format_t format, size_t samples);
  virtual int PcmHwParamsGetChannelsMin(const snd_pcm_hw_params_t* hw_params,
                                        unsigned int* min_channels);
  virtual int PcmHwParamsGetChannelsMax(const snd_pcm_hw_params_t* hw_params,
                                        unsigned int* max_channels);
  virtual int PcmHwParamsSetFormat(snd_pcm_t* handle,
                                   snd_pcm_hw_params_t* hw_params,
                                   snd_pcm_format_t format);
  virtual int PcmHwParamsSetAccess(snd_pcm_t* handle,
                                   snd_pcm_hw_params_t* hw_params,
                                   snd_pcm_access_t access);
  virtual int PcmHwParamsSetChannels(snd_pcm_t* handle,
                                     snd_pcm_hw_params_t* hw_params,
                                     unsigned int channels);
  virtual int PcmHwParamsSetBufferSizeNear(snd_pcm_t* handle,
                                           snd_pcm_hw_params_t* hw_params,
                                           snd_pcm_uframes_t* buffer_size);
  virtual int PcmHwParamsSetPeriodSizeNear(snd_pcm_t* handle,
                                           snd_pcm_hw_params_t* hw_params,
                                           snd_pcm_uframes_t* period_size,
                                           int* direction);
  virtual int PcmHwParams(snd_pcm_t* handle, snd_pcm_hw_params_t* hw_params);
  virtual void PcmHwParamsFree(snd_pcm_hw_params_t* hw_params);
  virtual int PcmSwParamsMalloc(snd_pcm_sw_params_t** sw_params);
  virtual int PcmSwParamsCurrent(snd_pcm_t* handle,
                                 snd_pcm_sw_params_t* sw_params);
  virtual int PcmSwParamsSetStartThreshold(snd_pcm_t* handle,
                                           snd_pcm_sw_params_t* sw_params,
                                           snd_pcm_uframes_t start_threshold);
  virtual int PcmSwParamsSetAvailMin(snd_pcm_t* handle,
                                     snd_pcm_sw_params_t* sw_params,
                                     snd_pcm_uframes_t period_size);
  virtual int PcmSwParams(snd_pcm_t* handle, snd_pcm_sw_params_t* sw_params);
  virtual void PcmSwParamsFree(snd_pcm_sw_params_t* sw_params);
  virtual const char* PcmName(snd_pcm_t* handle);
  virtual snd_pcm_sframes_t PcmAvailUpdate(snd_pcm_t* handle);
  virtual snd_pcm_state_t PcmState(snd_pcm_t* handle);
  virtual int PcmStart(snd_pcm_t* handle);

  virtual int MixerOpen(snd_mixer_t** mixer, int mode);
  virtual int MixerAttach(snd_mixer_t* mixer, const char* name);
  virtual int MixerElementRegister(snd_mixer_t* mixer,
                                   struct snd_mixer_selem_regopt* options,
                                   snd_mixer_class_t** classp);
  virtual void MixerFree(snd_mixer_t* mixer);
  virtual int MixerDetach(snd_mixer_t* mixer, const char* name);
  virtual int MixerClose(snd_mixer_t* mixer);
  virtual int MixerLoad(snd_mixer_t* mixer);
  virtual snd_mixer_elem_t* MixerFirstElem(snd_mixer_t* mixer);
  virtual snd_mixer_elem_t* MixerNextElem(snd_mixer_elem_t* elem);
  virtual int MixerSelemIsActive(snd_mixer_elem_t* elem);
  virtual const char* MixerSelemName(snd_mixer_elem_t* elem);
  virtual int MixerSelemSetCaptureVolumeAll(snd_mixer_elem_t* elem, long value);
  virtual int MixerSelemGetCaptureVolume(snd_mixer_elem_t* elem,
                                         snd_mixer_selem_channel_id_t channel,
                                         long* value);
  virtual int MixerSelemHasCaptureVolume(snd_mixer_elem_t* elem);
  virtual int MixerSelemGetCaptureVolumeRange(snd_mixer_elem_t* elem,
                                              long* min, long* max);
  virtual void* MixerElemGetCallbackPrivate(const snd_mixer_elem_t* obj);
  virtual void MixerElemSetCallback(snd_mixer_elem_t* obj,
                                    snd_mixer_elem_callback_t val);
  virtual void MixerElemSetCallbackPrivate(snd_mixer_elem_t* obj, void* val);
  virtual snd_mixer_elem_t* MixerFindSelem(snd_mixer_t* mixer,
                                           const snd_mixer_selem_id_t* id);
  virtual int MixerHandleEvents(snd_mixer_t* mixer);
  virtual int MixerPollDescriptors(snd_mixer_t* mixer,
                                   struct pollfd* pfds,
                                   unsigned int space);
  virtual int MixerPollDescriptorsCount(snd_mixer_t* mixer);
  virtual int MixerSelemGetPlaybackSwitch(snd_mixer_elem_t* elem,
                                          snd_mixer_selem_channel_id_t channel,
                                          int* value);
  virtual int MixerSelemGetPlaybackVolume(snd_mixer_elem_t* elem,
                                          snd_mixer_selem_channel_id_t channel,
                                          long* value);
  virtual int MixerSelemGetPlaybackVolumeRange(snd_mixer_elem_t* elem,
                                               long* min,
                                               long* max);
  virtual int MixerSelemHasPlaybackSwitch(snd_mixer_elem_t* elem);
  virtual void MixerSelemIdSetIndex(snd_mixer_selem_id_t* obj,
                                    unsigned int val);
  virtual void MixerSelemIdSetName(snd_mixer_selem_id_t* obj, const char* val);
  virtual int MixerSelemSetPlaybackSwitch(snd_mixer_elem_t* elem,
                                          snd_mixer_selem_channel_id_t channel,
                                          int value);
  virtual int MixerSelemSetPlaybackVolumeAll(snd_mixer_elem_t* elem,
                                             long value);
  virtual int MixerSelemIdMalloc(snd_mixer_selem_id_t** ptr);
  virtual void MixerSelemIdFree(snd_mixer_selem_id_t* obj);

  virtual const char* StrError(int errnum);

  DISALLOW_COPY_AND_ASSIGN(AlsaWrapper);
};

}  // namespace media

#endif  // MEDIA_AUDIO_ALSA_ALSA_WRAPPER_H_
