// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d_accelerator.h"

#include "media/base/media_log.h"

namespace media {

D3DAccelerator::D3DAccelerator(
    D3D11VideoDecoderClient* client,
    MediaLog* media_log,
    ComD3D11VideoDevice video_device,
    std::unique_ptr<VideoContextWrapper> video_context)
    : client_(client),
      media_log_(media_log),
      video_device_(std::move(video_device)),
      video_context_(std::move(video_context)) {
  DCHECK(client);
  DCHECK(media_log_);
  client->SetDecoderCB(base::BindRepeating(&D3DAccelerator::SetVideoDecoder,
                                           base::Unretained(this)));
}

D3DAccelerator::~D3DAccelerator() = default;

void D3DAccelerator::RecordFailure(base::StringPiece reason,
                                   D3D11Status::Codes code) const {
  DLOG(ERROR) << reason;
  MEDIA_LOG(ERROR, media_log_) << reason;
}

void D3DAccelerator::RecordFailure(base::StringPiece reason,
                                   D3D11Status::Codes code,
                                   HRESULT hr) const {
  DCHECK(FAILED(hr));
  std::string hr_string = logging::SystemErrorCodeToString(hr);
  if (!base::IsStringUTF8AllowingNoncharacters(hr_string)) {
    hr_string = "WARNING: system message could not be rendered!";
  }
  DLOG(ERROR) << reason << ": " << hr_string;
  MEDIA_LOG(ERROR, media_log_) << reason << ": " << hr_string;
}

void D3DAccelerator::SetVideoDecoder(ComD3D11VideoDecoder video_decoder) {
  video_decoder_ = std::move(video_decoder);
}

}  // namespace media
