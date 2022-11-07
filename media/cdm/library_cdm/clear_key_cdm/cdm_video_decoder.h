// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CDM_VIDEO_DECODER_H_
#define MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CDM_VIDEO_DECODER_H_

#include <stdint.h>

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "media/base/decoder_buffer.h"
#include "media/base/decoder_status.h"
#include "media/cdm/api/content_decryption_module.h"

namespace media {

class CdmHostProxy;

class CdmVideoDecoder {
 public:
  using CdmVideoFrame = cdm::VideoFrame_2;

  virtual ~CdmVideoDecoder() = default;
  virtual DecoderStatus Initialize(const cdm::VideoDecoderConfig_3& config) = 0;
  virtual void Deinitialize() = 0;
  virtual void Reset() = 0;
  virtual cdm::Status Decode(scoped_refptr<DecoderBuffer> buffer,
                             CdmVideoFrame* decoded_frame) = 0;
};

// Creates a CdmVideoDecoder based on the |config|. Returns nullptr if no
// decoder can be created.
std::unique_ptr<CdmVideoDecoder> CreateVideoDecoder(
    CdmHostProxy* cdm_host_proxy,
    const cdm::VideoDecoderConfig_3& config);

}  // namespace media

#endif  // MEDIA_CDM_LIBRARY_CDM_CLEAR_KEY_CDM_CDM_VIDEO_DECODER_H_
