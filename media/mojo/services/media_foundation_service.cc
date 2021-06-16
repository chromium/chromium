// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_foundation_service.h"

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "media/cdm/win/media_foundation_cdm_module.h"
#include "media/media_buildflags.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/key_system_support.mojom.h"
#include "media/mojo/services/interface_factory_impl.h"

namespace media {

MediaFoundationService::MediaFoundationService(
    mojo::PendingReceiver<mojom::MediaFoundationService> receiver,
    const base::FilePath& user_data_dir)
    : receiver_(this, std::move(receiver)), mojo_media_client_(user_data_dir) {
  DVLOG(1) << __func__;
  mojo_media_client_.Initialize();
}

MediaFoundationService::~MediaFoundationService() {
  DVLOG(1) << __func__;
}

void MediaFoundationService::IsKeySystemSupported(
    const std::string& key_system,
    IsKeySystemSupportedCallback callback) {
  DVLOG(2) << __func__;
  // TODO(crbug.com/1115687): Implement MediaFoundation-based key system support
  // query.
  NOTIMPLEMENTED();
  std::move(callback).Run(false, nullptr);
}

void MediaFoundationService::CreateInterfaceFactory(
    mojo::PendingReceiver<mojom::InterfaceFactory> receiver,
    mojo::PendingRemote<mojom::FrameInterfaceFactory> frame_interfaces) {
  DVLOG(2) << __func__;
  interface_factory_receivers_.Add(
      std::make_unique<InterfaceFactoryImpl>(std::move(frame_interfaces),
                                             &mojo_media_client_),
      std::move(receiver));
}

}  // namespace media
