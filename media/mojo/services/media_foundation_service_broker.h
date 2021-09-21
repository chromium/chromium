// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_BROKER_H_
#define MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_BROKER_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "media/mojo/mojom/media_foundation_service.mojom.h"
#include "media/mojo/services/media_foundation_service.h"
#include "media/mojo/services/media_mojo_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace media {

class MEDIA_MOJO_EXPORT MediaFoundationServiceBroker final
    : public mojom::MediaFoundationServiceBroker {
 public:
  // The MediaFoundationServiceBroker process is NOT sandboxed after
  // startup. The `ensure_sandboxed_cb` must be called after necessary
  // initialization to ensure the process is sandboxed.
  MediaFoundationServiceBroker(
      mojo::PendingReceiver<mojom::MediaFoundationServiceBroker> receiver,
      base::OnceClosure ensure_sandboxed_cb);
  MediaFoundationServiceBroker(const MediaFoundationServiceBroker&) = delete;
  MediaFoundationServiceBroker operator=(const MediaFoundationServiceBroker&) =
      delete;
  ~MediaFoundationServiceBroker() final;

  // mojom::MediaFoundationServiceBroker implementation:
  void GetService(const base::FilePath& cdm_path,
                  mojo::PendingReceiver<mojom::MediaFoundationService>
                      service_receiver) final;

 private:
  mojo::Receiver<mojom::MediaFoundationServiceBroker> receiver_;
  base::OnceClosure ensure_sandboxed_cb_;
  std::unique_ptr<MediaFoundationService> media_foundation_service_;
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MEDIA_FOUNDATION_SERVICE_BROKER_H_
