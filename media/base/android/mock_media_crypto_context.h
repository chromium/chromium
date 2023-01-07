// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_ANDROID_MOCK_MEDIA_CRYPTO_CONTEXT_H_
#define MEDIA_BASE_ANDROID_MOCK_MEDIA_CRYPTO_CONTEXT_H_

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

  MockMediaCryptoContext(const MockMediaCryptoContext&) = delete;
  MockMediaCryptoContext& operator=(const MockMediaCryptoContext&) = delete;

  ~MockMediaCryptoContext() override;

  // CdmContext implementation.
  MediaCryptoContext* GetMediaCryptoContext() override;

  // MediaCryptoContext implementation.
  void SetMediaCryptoReadyCB(
      MediaCryptoReadyCB media_crypto_ready_cb) override {
    SetMediaCryptoReadyCB_(media_crypto_ready_cb);
  }
  MOCK_METHOD1(SetMediaCryptoReadyCB_,
               void(MediaCryptoReadyCB& media_crypto_ready_cb));

  MediaCryptoReadyCB media_crypto_ready_cb_;

  // To be set to true when |media_crypto_ready_cb_| is consumed and run.
  bool ran_media_crypto_ready_cb_ = false;

 private:
  bool has_media_crypto_context_;
};

}  // namespace media

#endif  // MEDIA_BASE_ANDROID_MOCK_MEDIA_CRYPTO_CONTEXT_H_
