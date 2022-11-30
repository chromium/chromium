// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_SOURCE_WRAPPER_H_
#define MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_SOURCE_WRAPPER_H_

#include <mfapi.h>
#include <mfidl.h>
#include <wrl.h>

#include <map>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/media_resource.h"
#include "media/base/win/media_foundation_cdm_proxy.h"
#include "media/renderers/win/media_foundation_stream_wrapper.h"

namespace media {

class MediaLog;

// IMFMediaSource implementation
// (https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfmediasource)
// based on the given |media_resource|.
// Please also refer to "Writing a Custom Media Source" from
// https://docs.microsoft.com/en-us/windows/win32/medfound/writing-a-custom-media-source
//
// Note: The methods in this class can be called on two different threads -
//       Chromium thread and MF threadpool thread.
//
class MediaFoundationSourceWrapper
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaSource,
          IMFTrustedInput,
          IMFGetService,
          IMFRateSupport,
          IMFRateControl> {
 public:
  MediaFoundationSourceWrapper();
  ~MediaFoundationSourceWrapper() override;

  // This is only called on |task_runner|.
  void DetachResource();

  HRESULT RuntimeClassInitialize(
      MediaResource* media_resource,
      MediaLog* media_log,
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Note: All COM interface (IMFXxx) methods are called on the MF threadpool
  //       threads and the calls are serialized

  // IMFMediaSource
  IFACEMETHODIMP GetCharacteristics(DWORD* characteristics) override;
  IFACEMETHODIMP CreatePresentationDescriptor(
      IMFPresentationDescriptor** presentation_descriptor_out) override;
  IFACEMETHODIMP Start(IMFPresentationDescriptor* presentation_descriptor,
                       const GUID* guid_time_format,
                       const PROPVARIANT* start_position) override;
  IFACEMETHODIMP Stop() override;
  IFACEMETHODIMP Pause() override;
  IFACEMETHODIMP Shutdown() override;

  // IMFMediaEventGenerator
  // Note: IMFMediaSource inherits IMFMediaEventGenerator.
  IFACEMETHODIMP GetEvent(DWORD flags, IMFMediaEvent** event_out) override;
  IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* callback,
                               IUnknown* state) override;
  IFACEMETHODIMP EndGetEvent(IMFAsyncResult* result,
                             IMFMediaEvent** event_out) override;
  IFACEMETHODIMP QueueEvent(MediaEventType type,
                            REFGUID extended_type,
                            HRESULT status,
                            const PROPVARIANT* value) override;

  // IMFTrustedInput
  IFACEMETHODIMP GetInputTrustAuthority(DWORD stream_id,
                                        REFIID riid,
                                        IUnknown** object_out) override;

  // IMFGetService
  IFACEMETHODIMP GetService(REFGUID guid_service,
                            REFIID riid,
                            LPVOID* result) override;

  // IMFRateSupport
  IFACEMETHODIMP GetSlowestRate(MFRATE_DIRECTION direction,
                                BOOL supports_thinning,
                                float* rate) override;
  IFACEMETHODIMP GetFastestRate(MFRATE_DIRECTION direction,
                                BOOL supports_thinning,
                                float* rate) override;
  IFACEMETHODIMP IsRateSupported(BOOL supports_thinning,
                                 float new_rate,
                                 float* supported_rate) override;

  // IMFRateControl
  IFACEMETHODIMP SetRate(BOOL supports_thinning, float rate) override;
  IFACEMETHODIMP GetRate(BOOL* supports_thinning, float* rate) override;

  // Number of streams from |media_streams_|. Can be invoked from Chromium
  // thread or MF threadpool thread.
  uint32_t StreamCount() const;

  // The following methods are only invoked from Chromium thread.

  // Send MEEndOfPresentation event if necessary.
  void CheckForEndOfPresentation();
  // |media_streams_| has encrypted stream or not.
  bool HasEncryptedStream() const;
  // Set the internal |cdm_proxy_|.
  void SetCdmProxy(scoped_refptr<MediaFoundationCdmProxy> cdm_proxy);
  // Set the internal |video_stream_enabled_|. Returns true if we are
  // re-enabling a video stream which has previously reached EOS.
  bool SetVideoStreamEnabled(bool enabled);
  // Flused the queued buffers from each stream of |media_streams_|.
  void FlushStreams();

 private:
  // Select first audio stream and first video stream.
  HRESULT SelectDefaultStreams(
      const DWORD stream_desc_count,
      IMFPresentationDescriptor* presentation_descriptor);

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  std::vector<Microsoft::WRL::ComPtr<MediaFoundationStreamWrapper>>
      media_streams_;

  // |mf_media_event_queue_| is safe to be called on any thread.
  Microsoft::WRL::ComPtr<IMFMediaEventQueue> mf_media_event_queue_;

  // The proxy interface to communicate with MFCdm.
  scoped_refptr<MediaFoundationCdmProxy> cdm_proxy_;

  bool video_stream_enabled_ = true;
  float current_rate_ = 0.0f;
  bool presentation_ended_ = false;

  enum class State {
    kInitialized,
    kStarted,
    kStopped,
    kPaused,
    kShutdown
  } state_ = State::kInitialized;
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_FOUNDATION_SOURCE_WRAPPER_H_
