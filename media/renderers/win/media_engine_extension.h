// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_RENDERERS_WIN_MEDIA_ENGINE_EXTENSION_H_
#define MEDIA_RENDERERS_WIN_MEDIA_ENGINE_EXTENSION_H_

#include <mfapi.h>
#include <mfmediaengine.h>
#include <wrl.h>

#include "base/synchronization/lock.h"

namespace media {

// Implement IMFMediaEngineExtension to load media source into the
// IMFMediaEngine. See details from:
// https://docs.microsoft.com/en-us/windows/win32/api/mfmediaengine/nn-mfmediaengine-imfmediaengineextension.
//
class MediaEngineExtension
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<
              Microsoft::WRL::RuntimeClassType::ClassicCom>,
          IMFMediaEngineExtension> {
 public:
  MediaEngineExtension();
  ~MediaEngineExtension() override;

  HRESULT RuntimeClassInitialize();

  // IMFMediaEngineExtension
  IFACEMETHODIMP CanPlayType(BOOL is_audio_only,
                             BSTR mime_type,
                             MF_MEDIA_ENGINE_CANPLAY* result) override;
  IFACEMETHODIMP BeginCreateObject(BSTR url_bstr,
                                   IMFByteStream* byte_stream,
                                   MF_OBJECT_TYPE type,
                                   IUnknown** cancel_cookie,
                                   IMFAsyncCallback* callback,
                                   IUnknown* state) override;
  IFACEMETHODIMP CancelObjectCreation(IUnknown* cancel_cookie) override;
  IFACEMETHODIMP EndCreateObject(IMFAsyncResult* result,
                                 IUnknown** ret_obj) override;

  HRESULT SetMediaSource(IUnknown* mf_media_source);
  void Shutdown();

 private:
  bool pending_create_object_ = false;

  // Need a lock to ensure thread safe operation between IMFMediaEngineExtension
  // method calls from MFMediaEngine threadpool thread and
  // SetMediaSource/Shutdown from MediaFoundationRenderer calling thread.
  base::Lock lock_;
  bool has_shutdown_ GUARDED_BY(lock_) = false;
  Microsoft::WRL::ComPtr<IUnknown> mf_media_source_ GUARDED_BY(lock_);
};

}  // namespace media

#endif  // MEDIA_RENDERERS_WIN_MEDIA_ENGINE_EXTENSION_H_
