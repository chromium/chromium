// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_WIN_MEDIA_FOUNDATION_CDM_PROXY_H_
#define MEDIA_BASE_WIN_MEDIA_FOUNDATION_CDM_PROXY_H_

#include <unknwn.h>

#include <mfobjects.h>
#include <stdint.h>
#include <windef.h>

#include "base/memory/ref_counted.h"

namespace media {

// Interface for the media pipeline to get information from MediaFoundationCdm.
// TODO(xhwang): Investigate whether this class needs to be ref-counted.
class MediaFoundationCdmProxy
    : public base::RefCountedThreadSafe<MediaFoundationCdmProxy> {
 public:
  // Used by MediaFoundationProtectionManager to get
  // ABI::Windows::Media::Protection::IMediaProtectionPMPServer to implement
  // ABI::Windows::Media::Protection::IMediaProtectionManager::get_Properties
  // https://docs.microsoft.com/en-us/uwp/api/windows.media.protection.mediaprotectionmanager
  virtual HRESULT GetPMPServer(REFIID riid, LPVOID* object_result) = 0;

  // Used by MediaFoundationSourceWrapper to implement
  // IMFTrustedInput::GetInputTrustAuthority as in
  // https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imftrustedinput
  //
  // |content_init_data| is optional initialization data as in
  // https://www.w3.org/TR/encrypted-media/#initialization-data
  virtual HRESULT GetInputTrustAuthority(uint32_t stream_id,
                                         uint32_t stream_count,
                                         const uint8_t* content_init_data,
                                         uint32_t content_init_data_size,
                                         REFIID riid,
                                         IUnknown** object_out) = 0;

  // When the media Renderer is suspended, `MediaFoundationSourceWrapper`
  // provides its last set of key IDs using `SetLastKeyId()` when it is
  // destructed. Then during resume, the new `MediaFoundationSourceWrapper`
  // calls `RefreshTrustedInput()` to let the CDM use the key IDs information to
  // perform some optimization.
  virtual HRESULT SetLastKeyId(uint32_t stream_id, REFGUID key_id) = 0;
  virtual HRESULT RefreshTrustedInput() = 0;

  // Used by MediaFoundationProtectionManager to implement
  // IMFContentProtectionManager::BeginEnableContent as in
  // https://msdn.microsoft.com/en-us/windows/ms694217(v=vs.71)
  //
  // `result` is used to obtain the result of an asynchronous operation as in
  // https://docs.microsoft.com/en-us/windows/win32/api/mfobjects/nn-mfobjects-imfasyncresult
  virtual HRESULT ProcessContentEnabler(IUnknown* request,
                                        IMFAsyncResult* result) = 0;

  // Notify the CDM on DRM_E_TEE_INVALID_HWDRM_STATE (0x8004cd12), which happens
  // in cases like OS Sleep. In this case, the CDM should close all sessions
  // because they are in bad state.
  virtual void OnHardwareContextReset() = 0;

  // Notify the CDM that significant playback (e.g. >1 minutes) has happened.
  virtual void OnSignificantPlayback() = 0;

  // Notify the CDM that playback error happened.
  virtual void OnPlaybackError(HRESULT hresult) = 0;

 protected:
  friend base::RefCountedThreadSafe<MediaFoundationCdmProxy>;
  virtual ~MediaFoundationCdmProxy() = default;
};

}  // namespace media

#endif  // MEDIA_BASE_WIN_MEDIA_FOUNDATION_CDM_PROXY_H_
