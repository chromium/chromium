// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/mock_media_crypto_context.h"

#include "base/test/gmock_move_support.h"

using ::testing::_;

namespace media {

MockMediaCryptoContext::MockMediaCryptoContext(bool has_media_crypto_context)
    : has_media_crypto_context_(has_media_crypto_context) {
  if (!has_media_crypto_context_)
    return;

  ON_CALL(*this, SetMediaCryptoReadyCB_(_))
      .WillByDefault(MoveArg<0>(&media_crypto_ready_cb_));
}

MockMediaCryptoContext::~MockMediaCryptoContext() {}

MediaCryptoContext* MockMediaCryptoContext::GetMediaCryptoContext() {
  return has_media_crypto_context_ ? this : nullptr;
}

}  // namespace media
