// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_PREFERENCES_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_PREFERENCES_H_

#include "base/functional/callback_forward.h"
#include "media/mojo/mojom/media_foundation_preferences.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "url/gurl.h"

class MEDIA_MOJO_EXPORT MediaFoundationPreferencesImpl final
    : public media::mojom::MediaFoundationPreferences {
 public:
  using IsHardwareSecureDecryptionAllowedCB =
      base::RepeatingCallback<bool(const GURL&)>;

  MediaFoundationPreferencesImpl(const GURL& site,
                                 IsHardwareSecureDecryptionAllowedCB cb);

  MediaFoundationPreferencesImpl(const MediaFoundationPreferencesImpl&) =
      delete;
  MediaFoundationPreferencesImpl& operator=(
      const MediaFoundationPreferencesImpl&) = delete;

  ~MediaFoundationPreferencesImpl() override;

  static void Create(
      const GURL& site,
      IsHardwareSecureDecryptionAllowedCB cb,
      mojo::PendingReceiver<media::mojom::MediaFoundationPreferences> receiver);

  void IsHardwareSecureDecryptionAllowed(
      IsHardwareSecureDecryptionAllowedCallback cb) override;

 private:
  GURL site_;
  IsHardwareSecureDecryptionAllowedCB is_hardware_secure_decryption_allowed_cb_;
};

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_PREFERENCES_H_
