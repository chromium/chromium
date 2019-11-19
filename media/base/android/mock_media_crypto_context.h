// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_MEDIA_CRYPTO_CONTEXT_H_
#define MEDIA_BASE_ANDROID_MOCK_MEDIA_CRYPTO_CONTEXT_H_

#include <memory>

#include "base/macros.h"
#include "media/base/android/media_crypto_context.h"
#include "media/base/cdm_context.h"
#include "media/base/media_export.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

class MEDIA_EXPORT MockMediaCryptoContext
    : public CdmContext,
      public testing::NiceMock<MediaCryptoContext> {
 public:
  explicit MockMediaCryptoContext(bool has_media_crypto_context);
  ~MockMediaCryptoContext() override;

  // CdmContext implementation.
  MediaCryptoContext* GetMediaCryptoContext() override;

  // MediaCryptoContext implementation.
  MOCK_METHOD2(RegisterPlayer,
               int(const base::Closure& new_key_cb,
                   const base::Closure& cdm_unset_cb));
  MOCK_METHOD1(UnregisterPlayer, void(int registration_id));
  void SetMediaCryptoReadyCB(
      MediaCryptoReadyCB media_crypto_ready_cb) override {
    SetMediaCryptoReadyCB_(media_crypto_ready_cb);
  }
  MOCK_METHOD1(SetMediaCryptoReadyCB_,
               void(MediaCryptoReadyCB& media_crypto_ready_cb));

  static constexpr int kRegistrationId = 1000;

  base::Closure new_key_cb;
  base::Closure cdm_unset_cb;
  MediaCryptoReadyCB media_crypto_ready_cb;
  // To be set to true when |media_crypto_ready_cb| is consumed and run.
  bool ran_media_crypto_ready_cb = false;

 private:
  bool has_media_crypto_context_;
  DISALLOW_COPY_AND_ASSIGN(MockMediaCryptoContext);
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_MEDIA_CRYPTO_CONTEXT_H_
