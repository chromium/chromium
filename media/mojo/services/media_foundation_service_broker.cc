// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_service_broker.h"

#include "base/logging.h"
#include "media/cdm/win/media_foundation_cdm_module.h"

namespace media {

MediaFoundationServiceBroker::MediaFoundationServiceBroker(
    mojo::PendingReceiver<mojom::MediaFoundationServiceBroker> receiver,
    base::OnceClosure ensure_sandboxed_cb)
    : receiver_(this, std::move(receiver)),
      ensure_sandboxed_cb_(std::move(ensure_sandboxed_cb)) {}

MediaFoundationServiceBroker::~MediaFoundationServiceBroker() = default;

void MediaFoundationServiceBroker::GetService(
    const base::FilePath& cdm_path,
    mojo::PendingReceiver<mojom::MediaFoundationService> service_receiver) {
  DVLOG(1) << __func__ << ": cdm_path=" << cdm_path;

  if (media_foundation_service_) {
    DVLOG(1) << __func__ << ": MediaFoundationService can only be bound once";
    return;
  }

  MediaFoundationCdmModule::GetInstance()->Initialize(cdm_path);
  std::move(ensure_sandboxed_cb_).Run();

  media_foundation_service_ =
      std::make_unique<MediaFoundationService>(std::move(service_receiver));
}

}  // namespace media
