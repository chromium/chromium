// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_IMPL_H_
#define MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_IMPL_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/media_export.h"

namespace media {

class MediaDrmBridge;

// Implementation of MediaCryptoContext.
//
// The registered callbacks will be fired on the thread |media_drm_bridge_| is
// running on.
class MEDIA_EXPORT MediaCryptoContextImpl final : public MediaCryptoContext {
 public:
  // The |media_drm_bridge| owns |this| and is guaranteed to outlive |this|.
  explicit MediaCryptoContextImpl(MediaDrmBridge* media_drm_bridge);

  MediaCryptoContextImpl(const MediaCryptoContextImpl&) = delete;
  MediaCryptoContextImpl& operator=(const MediaCryptoContextImpl&) = delete;

  ~MediaCryptoContextImpl() override;

  // MediaCryptoContext implementation.
  void SetMediaCryptoReadyCB(MediaCryptoReadyCB media_crypto_ready_cb) override;

 private:
  const raw_ptr<MediaDrmBridge> media_drm_bridge_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_IMPL_H_
