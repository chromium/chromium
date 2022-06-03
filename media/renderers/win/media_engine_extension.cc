// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/renderers/win/media_engine_extension.h"

#include <mferror.h>

#include "media/base/win/mf_helpers.h"

namespace media {

using Microsoft::WRL::ComPtr;

MediaEngineExtension::MediaEngineExtension() = default;
MediaEngineExtension::~MediaEngineExtension() = default;

HRESULT MediaEngineExtension::RuntimeClassInitialize() {
  DVLOG_FUNC(1);
  return S_OK;
}

HRESULT MediaEngineExtension::CanPlayType(BOOL is_audio_only,
                                          BSTR mime_type,
                                          MF_MEDIA_ENGINE_CANPLAY* result) {
  // We use MF_MEDIA_ENGINE_EXTENSION to resolve as custom media source for
  // MFMediaEngine, MIME types are not used.
  *result = MF_MEDIA_ENGINE_CANPLAY_NOT_SUPPORTED;
  return S_OK;
}

HRESULT MediaEngineExtension::BeginCreateObject(BSTR url_bstr,
                                                IMFByteStream* byte_stream,
                                                MF_OBJECT_TYPE type,
                                                IUnknown** cancel_cookie,
                                                IMFAsyncCallback* callback,
                                                IUnknown* state) {
  DVLOG_FUNC(1) << "type=" << type;

  if (cancel_cookie) {
    // We don't support a cancel cookie.
    *cancel_cookie = nullptr;
  }
  ComPtr<IUnknown> local_source;
  {
    base::AutoLock lock(lock_);
    if (has_shutdown_) {
      return MF_E_SHUTDOWN;
    }
    local_source = mf_media_source_;
  }

  if (type == MF_OBJECT_MEDIASOURCE) {
    DVLOG_FUNC(2) << "Begin to resolve |mf_media_source_|";
    DCHECK(local_source) << "Media Source should have been set";

    ComPtr<IMFAsyncResult> async_result;
    RETURN_IF_FAILED(MFCreateAsyncResult(local_source.Get(), callback, state,
                                         &async_result));
    RETURN_IF_FAILED(async_result->SetStatus(S_OK));
    pending_create_object_ = true;
    // Invoke the callback synchronously since no outstanding work is required.
    RETURN_IF_FAILED(callback->Invoke(async_result.Get()));
  } else {
    // MediaEngine will try to resolve with different |type|.
    return MF_E_UNEXPECTED;
  }

  return S_OK;
}

HRESULT MediaEngineExtension::CancelObjectCreation(
    __in IUnknown* cancel_cookie) {
  DVLOG_FUNC(1);

  return MF_E_UNEXPECTED;
}

HRESULT MediaEngineExtension::EndCreateObject(__in IMFAsyncResult* result,
                                              __deref_out IUnknown** ret_obj) {
  DVLOG_FUNC(1);

  *ret_obj = nullptr;
  if (!pending_create_object_)
    return MF_E_UNEXPECTED;

  DVLOG_FUNC(2) << "End to resolve |mf_media_source_|";
  RETURN_IF_FAILED(result->GetStatus());
  RETURN_IF_FAILED(result->GetObject(ret_obj));
  pending_create_object_ = false;
  return S_OK;
}

HRESULT MediaEngineExtension::SetMediaSource(IUnknown* mf_media_source) {
  DVLOG_FUNC(1);

  base::AutoLock lock(lock_);
  if (has_shutdown_)
    return MF_E_SHUTDOWN;
  mf_media_source_ = mf_media_source;
  return S_OK;
}

// Break cycles.
void MediaEngineExtension::Shutdown() {
  DVLOG_FUNC(1);

  base::AutoLock lock(lock_);
  if (!has_shutdown_) {
    mf_media_source_.Reset();
    has_shutdown_ = true;
  }
}

}  // namespace media
