// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/stable_video_decoder_factory.h"

#include "build/chromeos_buildflags.h"
#include "content/public/browser/service_process_host.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace content {

void LaunchStableVideoDecoderFactory(
    mojo::PendingReceiver<media::stable::mojom::StableVideoDecoderFactory>
        receiver) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // For LaCrOS, we need to use crosapi to establish a
  // StableVideoDecoderFactory connection to ash-chrome.
  auto* lacros_service = chromeos::LacrosService::Get();
  if (lacros_service && lacros_service->IsStableVideoDecoderFactoryAvailable())
    lacros_service->BindStableVideoDecoderFactory(std::move(receiver));
#else
  ServiceProcessHost::Launch(
      std::move(receiver),
      ServiceProcessHost::Options().WithDisplayName("Video Decoder").Pass());
#endif
}

}  // namespace content
