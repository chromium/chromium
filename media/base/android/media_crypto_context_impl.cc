// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_crypto_context_impl.h"

#include "media/base/android/media_drm_bridge.h"

namespace media {

MediaCryptoContextImpl::MediaCryptoContextImpl(MediaDrmBridge* media_drm_bridge)
    : media_drm_bridge_(media_drm_bridge) {
  DCHECK(media_drm_bridge_);
}

MediaCryptoContextImpl::~MediaCryptoContextImpl() = default;

void MediaCryptoContextImpl::SetMediaCryptoReadyCB(
    MediaCryptoReadyCB media_crypto_ready_cb) {
  media_drm_bridge_->SetMediaCryptoReadyCB(std::move(media_crypto_ready_cb));
}

}  // namespace media
