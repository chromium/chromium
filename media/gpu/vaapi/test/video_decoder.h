// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_VAAPI_TEST_VIDEO_DECODER_H_
#define MEDIA_GPU_VAAPI_TEST_VIDEO_DECODER_H_

#include "base/memory/raw_ref.h"
#include "media/gpu/vaapi/test/shared_va_surface.h"

namespace media {
namespace vaapi_test {

class VaapiDevice;

// VideoDecoder specifies an interface for frame-by-frame libva-based decoding
// with different profiles.
class VideoDecoder {
 public:
  // Result of decoding the current frame.
  enum Result {
    kOk,
    kEOStream,
  };

  VideoDecoder(const VaapiDevice& va_device,
               SharedVASurface::FetchPolicy fetch_policy);
  // Implementations of VideoDecoder are expected to handle the destruction of
  // |last_decoded_surface_| and in particular ensure it is done in the right
  // order with respect to the other VAAPI objects.
  virtual ~VideoDecoder();

  VideoDecoder(const VideoDecoder&) = delete;
  VideoDecoder& operator=(const VideoDecoder&) = delete;

  // Decodes the next frame in this decoder. Errors are fatal.
  virtual Result DecodeNextFrame() = 0;

  // Outputs the last decoded frame to a PNG at the given |path|.
  // "Last decoded" includes frames specified not to be shown as well as
  // frames referring to already existing/previously decoded frames.
  // It is therefore possible that the images outputted do not exactly match
  // what is displayed by playing the video stream directly.
  void LastDecodedFrameToPNG(const std::string& path) const {
    last_decoded_surface_->SaveAsPNG(fetch_policy_, path);
  }

  // Computes the MD5 sum of the last decoded frame and returns a human-readable
  // representation.
  std::string LastDecodedFrameMD5Sum() const {
    return last_decoded_surface_->GetMD5Sum(fetch_policy_);
  }

  // Returns whether the last decoded frame was visible.
  bool LastDecodedFrameVisible() const { return last_decoded_frame_visible_; }

 protected:
  // VA handles.
  const raw_ref<const VaapiDevice> va_device_;
  scoped_refptr<SharedVASurface> last_decoded_surface_;

  // Whether the last decoded frame was visible.
  bool last_decoded_frame_visible_ = false;

  // How to fetch image data from VASurfaces decoded into by this decoder.
  const SharedVASurface::FetchPolicy fetch_policy_;
};

}  // namespace vaapi_test
}  // namespace media

#endif  // MEDIA_GPU_VAAPI_TEST_VIDEO_DECODER_H_
