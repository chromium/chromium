// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VP8_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_VP8_DECODER_H_

#include "base/memory/scoped_refptr.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/scoped_va_context.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/parsers/ivf_parser.h"
#include "media/parsers/vp8_parser.h"

namespace media {

namespace vaapi_test {

class ScopedVAConfig;
class ScopedVAContext;
class SharedVASurface;
class VaapiDevice;

// A Vp8Decoder decodes VP8-encoded IVF streams using direct libva calls.
class Vp8Decoder : public VideoDecoder {
 public:
  Vp8Decoder(std::unique_ptr<IvfParser> ivf_parser,
             const VaapiDevice& va_device,
             SharedVASurface::FetchPolicy fetch_policy);
  Vp8Decoder(const Vp8Decoder&) = delete;
  Vp8Decoder& operator=(const Vp8Decoder&) = delete;
  ~Vp8Decoder() override;

  // VideoDecoder implementation.
  VideoDecoder::Result DecodeNextFrame() override;

 private:
  enum ParseResult { kOk, kEOStream, kError };

  ParseResult ReadNextFrame(Vp8FrameHeader& vp8_frame_header);

  void FillVp8DataStructures(const Vp8FrameHeader& frame_hdr,
                             VAIQMatrixBufferVP8& iq_matrix_buf,
                             VAProbabilityDataBufferVP8& prob_buf,
                             VAPictureParameterBufferVP8& pic_param,
                             VASliceParameterBufferVP8& slice_param);

  void RefreshReferenceSlots(Vp8FrameHeader& vp8_frame_header,
                             scoped_refptr<SharedVASurface> surface);

  // VA handles.
  std::unique_ptr<ScopedVAConfig> va_config_;
  std::unique_ptr<ScopedVAContext> va_context_;

  // VP8-specific data.
  const std::unique_ptr<Vp8Parser> vp8_parser_;
  std::vector<scoped_refptr<SharedVASurface>> ref_frames_;

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;
};

}  // namespace vaapi_test

}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VP8_DECODER_H_
