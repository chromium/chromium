// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_preferences.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

MediaFoundationPreferencesImpl::MediaFoundationPreferencesImpl(
    IsHardwareSecureDecryptionDisabledCB cb)
    : is_hardware_decryption_disabled_cb_(cb) {}
MediaFoundationPreferencesImpl::~MediaFoundationPreferencesImpl() = default;

// static
void MediaFoundationPreferencesImpl::Create(
    IsHardwareSecureDecryptionDisabledCB cb,
    mojo::PendingReceiver<media::mojom::MediaFoundationPreferences> receiver) {
  DVLOG(2) << __func__;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaFoundationPreferencesImpl>(cb),
      std::move(receiver));
}

void MediaFoundationPreferencesImpl::IsHardwareSecureDecryptionPreferred(
    IsHardwareSecureDecryptionPreferredCallback callback) {
  std::move(callback).Run(!is_hardware_decryption_disabled_cb_.Run());
}
