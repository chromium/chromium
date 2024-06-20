// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VP9_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_VP9_DECODER_H_

#include "media/gpu/vaapi/test/video_decoder.h"
#include "media/parsers/vp9_parser.h"

namespace media {

class IvfParser;

namespace vaapi_test {

class ScopedVAConfig;
class ScopedVAContext;
class SharedVASurface;
class VaapiDevice;

// A Vp9Decoder decodes VP9-encoded IVF streams using direct libva calls.
class Vp9Decoder : public VideoDecoder {
 public:
  Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
             const VaapiDevice& va_device,
             SharedVASurface::FetchPolicy fetch_policy);
  Vp9Decoder(const Vp9Decoder&) = delete;
  Vp9Decoder& operator=(const Vp9Decoder&) = delete;
  ~Vp9Decoder() override;

  // VideoDecoder implementation.
  VideoDecoder::Result DecodeNextFrame() override;

 private:
  // Reads next frame from IVF stream and its size into |vp9_frame_header| and
  // |size| respectively.
  Vp9Parser::Result ReadNextFrame(Vp9FrameHeader& vp9_frame_header,
                                  gfx::Size& size);

  // Refreshes current reference frame slots to refer to |surface| according to
  // |refresh_frame_flags|.
  void RefreshReferenceSlots(uint8_t refresh_frame_flags,
                             scoped_refptr<SharedVASurface> surface);

  // VA handles.
  std::unique_ptr<ScopedVAConfig> va_config_;
  std::unique_ptr<ScopedVAContext> va_context_;

  // VP9-specific data.
  const std::unique_ptr<Vp9Parser> vp9_parser_;
  std::vector<scoped_refptr<SharedVASurface>> ref_frames_;

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VP9_DECODER_H_
