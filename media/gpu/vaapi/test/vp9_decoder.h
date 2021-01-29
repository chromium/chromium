// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VP9_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_VP9_DECODER_H_

#include "media/filters/ivf_parser.h"
#include "media/filters/vp9_parser.h"
#include "media/gpu/vaapi/test/scoped_va_config.h"
#include "media/gpu/vaapi/test/scoped_va_context.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"
#include "media/gpu/vaapi/test/vaapi_device.h"
#include "media/gpu/vaapi/test/video_decoder.h"

namespace media {
namespace vaapi_test {

// A Vp9Decoder decodes VP9-encoded IVF streams using direct libva calls.
class Vp9Decoder : public VideoDecoder {
 public:
  Vp9Decoder(std::unique_ptr<IvfParser> ivf_parser,
             const VaapiDevice& va_device);
  Vp9Decoder(const Vp9Decoder&) = delete;
  Vp9Decoder& operator=(const Vp9Decoder&) = delete;
  ~Vp9Decoder() override;

  // VideoDecoder implementation.
  VideoDecoder::Result DecodeNextFrame() override;
  void LastDecodedFrameToPNG(const std::string& path) override;
  std::string LastDecodedFrameMD5Sum() override;
  bool LastDecodedFrameVisible() override;

 private:
  // Reads next frame from IVF stream and its size into |vp9_frame_header| and
  // |size| respectively.
  Vp9Parser::Result ReadNextFrame(Vp9FrameHeader& vp9_frame_header,
                                  gfx::Size& size);

  // Refreshes current reference frame slots to refer to |surface| according to
  // |refresh_frame_flags|.
  void RefreshReferenceSlots(uint8_t refresh_frame_flags,
                             scoped_refptr<SharedVASurface> surface);

  // Parser for the IVF stream to decode.
  const std::unique_ptr<IvfParser> ivf_parser_;

  // VA handles.
  const VaapiDevice& va_device_;
  std::unique_ptr<ScopedVAConfig> va_config_;
  std::unique_ptr<ScopedVAContext> va_context_;
  scoped_refptr<SharedVASurface> last_decoded_surface_;

  // VP9-specific data.
  const std::unique_ptr<Vp9Parser> vp9_parser_;
  std::vector<scoped_refptr<SharedVASurface>> ref_frames_;

  // Whether the last decoded frame was visible.
  bool last_decoded_frame_visible_ = false;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VP9_DECODER_H_
