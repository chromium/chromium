// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIADRM_SUPPORT_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MEDIADRM_SUPPORT_SERVICE_H_

#include <string>

#include "media/mojo/mojom/mediadrm_support.mojom.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class MEDIA_MOJO_EXPORT MediaDrmSupportService final
    : public mojom::MediaDrmSupport {
 public:
  explicit MediaDrmSupportService(
      mojo::PendingReceiver<mojom::MediaDrmSupport> receiver);

  MediaDrmSupportService(const MediaDrmSupportService&) = delete;
  MediaDrmSupportService& operator=(const MediaDrmSupportService&) = delete;

  ~MediaDrmSupportService() final;

  // mojom::MediaDrmSupport interface
  void IsKeySystemSupported(const std::string& key_system,
                            IsKeySystemSupportedCallback callback) override;

 private:
  mojo::Receiver<mojom::MediaDrmSupport> receiver_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIADRM_SUPPORT_SERVICE_H_
