// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/stable_video_decoder_factory_service.h"

namespace media {

StableVideoDecoderFactoryService::StableVideoDecoderFactoryService() = default;
StableVideoDecoderFactoryService::~StableVideoDecoderFactoryService() = default;

void StableVideoDecoderFactoryService::BindReceiver(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoderFactory> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void StableVideoDecoderFactoryService::CreateStableVideoDecoder(
    mojo::PendingReceiver<stable::mojom::StableVideoDecoder> receiver) {
  // TODO(b/171813538): connect with the ash-chrome video decoding stack.
  // TODO(b/195769334): plumb OOP-VD.
  NOTIMPLEMENTED();
}

}  // namespace media
