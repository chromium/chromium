// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_preferences.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/logging.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

MediaFoundationPreferencesImpl::MediaFoundationPreferencesImpl(
    const GURL& site,
    IsHardwareSecureDecryptionAllowedCB cb)
    : site_(site), is_hardware_secure_decryption_allowed_cb_(cb) {}
MediaFoundationPreferencesImpl::~MediaFoundationPreferencesImpl() = default;

// static
void MediaFoundationPreferencesImpl::Create(
    const GURL& site,
    IsHardwareSecureDecryptionAllowedCB cb,
    mojo::PendingReceiver<media::mojom::MediaFoundationPreferences> receiver) {
  DVLOG(2) << __func__;

  mojo::MakeSelfOwnedReceiver(
      std::make_unique<MediaFoundationPreferencesImpl>(site, cb),
      std::move(receiver));
}

void MediaFoundationPreferencesImpl::IsHardwareSecureDecryptionAllowed(
    IsHardwareSecureDecryptionAllowedCallback cb) {
  DVLOG(2) << __func__;

  if (!is_hardware_secure_decryption_allowed_cb_) {
    std::move(cb).Run(true);
    return;
  }

  std::move(cb).Run(is_hardware_secure_decryption_allowed_cb_.Run(site_));
}
