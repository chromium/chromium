// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_ALSA_MOCK_ALSA_WRAPPER_H_
#define MEDIA_AUDIO_ALSA_MOCK_ALSA_WRAPPER_H_

#include "media/audio/alsa/alsa_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MockAlsaWrapper : public AlsaWrapper {
 public:
  MockAlsaWrapper();

  MockAlsaWrapper(const MockAlsaWrapper&) = delete;
  MockAlsaWrapper& operator=(const MockAlsaWrapper&) = delete;

  ~MockAlsaWrapper() override;

  MOCK_METHOD3(DeviceNameHint, int(int card, const char* iface, void*** hints));
  MOCK_METHOD2(DeviceNameGetHint, char*(const void* hint, const char* id));
  MOCK_METHOD1(DeviceNameFreeHint, int(void** hints));
  MOCK_METHOD1(CardNext, int(int* rcard));
  MOCK_METHOD4(PcmOpen,
               int(snd_pcm_t** handle,
                   const char* name,
                   snd_pcm_stream_t stream,
                   int mode));
  MOCK_METHOD1(PcmClose, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmPrepare, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmDrain, int(snd_pcm_t* handle));
  MOCK_METHOD1(PcmDrop, int(snd_pcm_t* handle));
  MOCK_METHOD2(PcmDelay, int(snd_pcm_t* handle, snd_pcm_sframes_t* delay));
  MOCK_METHOD1(PcmResume, int(snd_pcm_t* handle));
  MOCK_METHOD3(PcmWritei,
               snd_pcm_sframes_t(snd_pcm_t* handle,
                                 const void* buffer,
                                 snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmReadi,
               snd_pcm_sframes_t(snd_pcm_t* handle,
                                 void* buffer,
                                 snd_pcm_uframes_t size));
  MOCK_METHOD3(PcmRecover, int(snd_pcm_t* handle, int err, int silent));
  MOCK_METHOD7(PcmSetParams,
               int(snd_pcm_t* handle,
                   snd_pcm_format_t format,
                   snd_pcm_access_t access,
                   unsigned int channels,
                   unsigned int rate,
                   int soft_resample,
                   unsigned int latency));
  MOCK_METHOD3(PcmGetParams,
               int(snd_pcm_t* handle,
                   snd_pcm_uframes_t* buffer_size,
                   snd_pcm_uframes_t* period_size));
  MOCK_METHOD1(PcmHwParamsMalloc, int(snd_pcm_hw_params_t** hw_params));
  MOCK_METHOD2(PcmHwParamsAny,
               int(snd_pcm_t* handle, snd_pcm_hw_params_t* hw_params));
  MOCK_METHOD1(PcmHwParamsCanResume, int(snd_pcm_hw_params_t* hw_params));

  MOCK_METHOD3(PcmHwParamsSetRateResample,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   unsigned int value));
  MOCK_METHOD4(PcmHwParamsSetRateNear,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   unsigned int* rate,
                   int* direction));
  MOCK_METHOD3(PcmHwParamsTestFormat,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   snd_pcm_format_t format));
  MOCK_METHOD2(PcmFormatSize, int(snd_pcm_format_t format, size_t samples));
  MOCK_METHOD2(PcmHwParamsGetChannelsMin,
               int(const snd_pcm_hw_params_t* hw_params,
                   unsigned int* min_channels));
  MOCK_METHOD2(PcmHwParamsGetChannelsMax,
               int(const snd_pcm_hw_params_t* hw_params,
                   unsigned int* max_channels));
  MOCK_METHOD3(PcmHwParamsSetFormat,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   snd_pcm_format_t format));
  MOCK_METHOD3(PcmHwParamsSetAccess,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   snd_pcm_access_t access));
  MOCK_METHOD3(PcmHwParamsSetChannels,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   unsigned int channels));
  MOCK_METHOD3(PcmHwParamsSetBufferSizeNear,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   snd_pcm_uframes_t* buffer_size));
  MOCK_METHOD4(PcmHwParamsSetPeriodSizeNear,
               int(snd_pcm_t* handle,
                   snd_pcm_hw_params_t* hw_params,
                   snd_pcm_uframes_t* period_size,
                   int* direction));
  MOCK_METHOD2(PcmHwParams,
               int(snd_pcm_t* handle, snd_pcm_hw_params_t* hw_params));
  MOCK_METHOD1(PcmHwParamsFree, void(snd_pcm_hw_params_t* hw_params));
  MOCK_METHOD1(PcmSwParamsMalloc, int(snd_pcm_sw_params_t** sw_params));
  MOCK_METHOD2(PcmSwParamsCurrent,
               int(snd_pcm_t* handle, snd_pcm_sw_params_t* sw_params));
  MOCK_METHOD3(PcmSwParamsSetStartThreshold,
               int(snd_pcm_t* handle,
                   snd_pcm_sw_params_t* sw_params,
                   snd_pcm_uframes_t start_threshold));
  MOCK_METHOD3(PcmSwParamsSetAvailMin,
               int(snd_pcm_t* handle,
                   snd_pcm_sw_params_t* sw_params,
                   snd_pcm_uframes_t period_size));
  MOCK_METHOD2(PcmSwParams,
               int(snd_pcm_t* handle, snd_pcm_sw_params_t* sw_params));
  MOCK_METHOD1(PcmSwParamsFree, void(snd_pcm_sw_params_t* sw_params));
  MOCK_METHOD1(PcmName, const char*(snd_pcm_t* handle));
  MOCK_METHOD1(PcmAvailUpdate, snd_pcm_sframes_t(snd_pcm_t* handle));
  MOCK_METHOD1(PcmState, snd_pcm_state_t(snd_pcm_t* handle));
  MOCK_METHOD1(PcmStart, int(snd_pcm_t* handle));
  MOCK_METHOD2(MixerOpen, int(snd_mixer_t** mixer, int mode));
  MOCK_METHOD2(MixerAttach, int(snd_mixer_t* mixer, const char* name));
  MOCK_METHOD3(MixerElementRegister,
               int(snd_mixer_t* mixer,
                   struct snd_mixer_selem_regopt* options,
                   snd_mixer_class_t** classp));
  MOCK_METHOD1(MixerFree, void(snd_mixer_t* mixer));
  MOCK_METHOD2(MixerDetach, int(snd_mixer_t* mixer, const char* name));
  MOCK_METHOD1(MixerClose, int(snd_mixer_t* mixer));
  MOCK_METHOD1(MixerLoad, int(snd_mixer_t* mixer));
  MOCK_METHOD1(MixerFirstElem, snd_mixer_elem_t*(snd_mixer_t* mixer));
  MOCK_METHOD1(MixerNextElem, snd_mixer_elem_t*(snd_mixer_elem_t* elem));
  MOCK_METHOD1(MixerSelemIsActive, int(snd_mixer_elem_t* elem));
  MOCK_METHOD1(MixerSelemName, const char*(snd_mixer_elem_t* elem));
  MOCK_METHOD2(MixerSelemSetCaptureVolumeAll,
               int(snd_mixer_elem_t* elem, long value));
  MOCK_METHOD3(MixerSelemGetCaptureVolume,
               int(snd_mixer_elem_t* elem,
                   snd_mixer_selem_channel_id_t channel,
                   long* value));
  MOCK_METHOD1(MixerSelemHasCaptureVolume, int(snd_mixer_elem_t* elem));
  MOCK_METHOD3(MixerSelemGetCaptureVolumeRange,
               int(snd_mixer_elem_t* elem, long* min, long* max));
  MOCK_METHOD1(MixerElemGetCallbackPrivate, void*(const snd_mixer_elem_t* obj));
  MOCK_METHOD2(MixerElemSetCallback,
               void(snd_mixer_elem_t* obj, snd_mixer_elem_callback_t val));
  MOCK_METHOD2(MixerElemSetCallbackPrivate,
               void(snd_mixer_elem_t* obj, void* val));
  MOCK_METHOD2(MixerFindSelem,
               snd_mixer_elem_t*(snd_mixer_t* mixer,
                                 const snd_mixer_selem_id_t* id));
  MOCK_METHOD1(MixerHandleEvents, int(snd_mixer_t* mixer));
  MOCK_METHOD3(MixerPollDescriptors,
               int(snd_mixer_t* mixer,
                   struct pollfd* pfds,
                   unsigned int space));
  MOCK_METHOD1(MixerPollDescriptorsCount, int(snd_mixer_t* mixer));
  MOCK_METHOD3(MixerSelemGetPlaybackSwitch,
               int(snd_mixer_elem_t* elem,
                   snd_mixer_selem_channel_id_t channel,
                   int* value));
  MOCK_METHOD3(MixerSelemGetPlaybackVolume,
               int(snd_mixer_elem_t* elem,
                   snd_mixer_selem_channel_id_t channel,
                   long* value));
  MOCK_METHOD3(MixerSelemGetPlaybackVolumeRange,
               int(snd_mixer_elem_t* elem, long* min, long* max));
  MOCK_METHOD(int,
              MixerSelemAskPlaybackVolDb,
              (snd_mixer_elem_t * elem, long value, long* db_value),
              (override));
  MOCK_METHOD(int,
              MixerSelemAskPlaybackDbVol,
              (snd_mixer_elem_t * elem, long db_value, long* value),
              (override));
  MOCK_METHOD1(MixerSelemHasPlaybackSwitch, int(snd_mixer_elem_t* elem));
  MOCK_METHOD(int,
              MixerSelemHasPlaybackVolume,
              (snd_mixer_elem_t * elem),
              (override));
  MOCK_METHOD2(MixerSelemIdSetIndex,
               void(snd_mixer_selem_id_t* obj, unsigned int val));
  MOCK_METHOD2(MixerSelemIdSetName,
               void(snd_mixer_selem_id_t* obj, const char* val));
  MOCK_METHOD3(MixerSelemSetPlaybackSwitch,
               int(snd_mixer_elem_t* elem,
                   snd_mixer_selem_channel_id_t channel,
                   int value));
  MOCK_METHOD(int,
              MixerSelemSetPlaybackSwitchAll,
              (snd_mixer_elem_t * elem, int value),
              (override));
  MOCK_METHOD2(MixerSelemSetPlaybackVolumeAll,
               int(snd_mixer_elem_t* elem, long value));
  MOCK_METHOD1(MixerSelemIdMalloc, int(snd_mixer_selem_id_t** ptr));
  MOCK_METHOD1(MixerSelemIdFree, void(snd_mixer_selem_id_t* obj));
  MOCK_METHOD1(StrError, const char*(int errnum));
};

}  // namespace media

#endif  // MEDIA_AUDIO_ALSA_MOCK_ALSA_WRAPPER_H_
