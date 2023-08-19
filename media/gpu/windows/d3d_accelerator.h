// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_WINDOWS_D3D_ACCELERATOR_H_
#define MEDIA_GPU_WINDOWS_D3D_ACCELERATOR_H_

#include <memory>

#include "base/strings/string_piece.h"
#include "media/gpu/windows/d3d11_com_defs.h"
#include "media/gpu/windows/d3d11_status.h"
#include "media/gpu/windows/d3d11_video_decoder_client.h"

namespace media {

class MediaLog;

class D3DAccelerator {
 public:
  D3DAccelerator(D3D11VideoDecoderClient* client, MediaLog* media_log);
  virtual ~D3DAccelerator();

 protected:
  // Record a failure to DVLOG and |media_log_|.
  void RecordFailure(base::StringPiece reason, D3D11Status::Codes code) const;
  void RecordFailure(base::StringPiece reason,
                     D3D11Status::Codes code,
                     HRESULT hr) const;

  void SetVideoDecoderWrapper(
      std::unique_ptr<D3DVideoDecoderWrapper> video_decoder_wrapper);

  raw_ptr<D3D11VideoDecoderClient> client_;
  raw_ptr<MediaLog> media_log_ = nullptr;
  std::unique_ptr<D3DVideoDecoderWrapper> video_decoder_wrapper_;
};

}  // namespace media

#endif  // MEDIA_GPU_WINDOWS_D3D_ACCELERATOR_H_
