// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"  // nogncheck
#include "media/mojo/services/gpu_mojo_media_client.h"

namespace media {

std::unique_ptr<CdmFactory> CreatePlatformCdmFactory(
    mojom::FrameInterfaceFactory* frame_interfaces) {
  return std::make_unique<chromeos::ChromeOsCdmFactory>(frame_interfaces);
}

}  // namespace media