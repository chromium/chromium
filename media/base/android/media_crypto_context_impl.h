// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_IMPL_H_
#define MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_IMPL_H_

#include <jni.h>

#include <memory>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/media_export.h"

namespace media {

class MediaDrmBridge;

// Implementation of MediaCryptoContext.
//
// The registered callbacks will be fired on the thread |media_drm_bridge_| is
// running on.
class MEDIA_EXPORT MediaCryptoContextImpl : public MediaCryptoContext {
 public:
  // The |media_drm_bridge| owns |this| and is guaranteed to outlive |this|.
  explicit MediaCryptoContextImpl(MediaDrmBridge* media_drm_bridge);

  ~MediaCryptoContextImpl() final;

  // MediaCryptoContext implementation.
  void SetMediaCryptoReadyCB(MediaCryptoReadyCB media_crypto_ready_cb) final;

 private:
  MediaDrmBridge* const media_drm_bridge_;

  DISALLOW_COPY_AND_ASSIGN(MediaCryptoContextImpl);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_IMPL_H_
