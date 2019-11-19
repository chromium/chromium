// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_H_
#define MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "media/base/android/android_util.h"
#include "media/base/media_export.h"
#include "media/base/player_tracker.h"

namespace media {

// A class that provides MediaCrypto from MediaDrm to support decrypting and
// decoding of encrypted streams, typically by MediaCodec-based decoders.
//
// Methods can be called on any thread. The registered callbacks can be fired
// on any thread. The caller should make sure that the callbacks are posted to
// the correct thread.
//
// TODO(xhwang): Remove PlayerTracker interface.
class MEDIA_EXPORT MediaCryptoContext : public PlayerTracker {
 public:
  // Notification called when MediaCrypto object is ready.
  // Parameters:
  // |media_crypto| - global reference to MediaCrypto object. |media_crypto| is
  //                  always a non-null std::unique_ptr, but the JavaRef it
  //                  contains can point to a null object.
  // |requires_secure_video_codec| - true if secure video decoder is required.
  //                                 Should be ignored if |media_crypto|
  //                                 contains null MediaCrypto object.
  using MediaCryptoReadyCB =
      base::OnceCallback<void(JavaObjectPtr media_crypto,
                              bool requires_secure_video_codec)>;

  MediaCryptoContext() {}
  ~MediaCryptoContext() override {}

  virtual void SetMediaCryptoReadyCB(
      MediaCryptoReadyCB media_crypto_ready_cb) = 0;

  DISALLOW_COPY_AND_ASSIGN(MediaCryptoContext);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MEDIA_CRYPTO_CONTEXT_H_
