// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d_accelerator.h"

#include "media/base/media_log.h"

namespace media {

D3DAccelerator::D3DAccelerator(D3D11VideoDecoderClient* client,
                               MediaLog* media_log)
    : client_(client), media_log_(media_log) {
  DCHECK(client);
  DCHECK(media_log_);
  client->SetDecoderWrapperCB(base::BindRepeating(
      &D3DAccelerator::SetVideoDecoderWrapper, base::Unretained(this)));
}

D3DAccelerator::~D3DAccelerator() = default;

void D3DAccelerator::SetVideoDecoderWrapper(
    std::unique_ptr<D3DVideoDecoderWrapper> video_decoder_wrapper) {
  CHECK(video_decoder_wrapper);
  video_decoder_wrapper_ = std::move(video_decoder_wrapper);
}

}  // namespace media
