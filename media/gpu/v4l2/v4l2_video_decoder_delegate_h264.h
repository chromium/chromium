// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H264_H_
#define MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H264_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"

namespace media {

class V4L2Device;
class V4L2DecodeSurface;
class V4L2DecodeSurfaceHandler;
struct V4L2VideoDecoderDelegateH264Private;

class V4L2VideoDecoderDelegateH264 : public H264Decoder::H264Accelerator {
 public:
  using Status = H264Decoder::H264Accelerator::Status;

  explicit V4L2VideoDecoderDelegateH264(
      V4L2DecodeSurfaceHandler* surface_handler,
      V4L2Device* device);

  V4L2VideoDecoderDelegateH264(const V4L2VideoDecoderDelegateH264&) = delete;
  V4L2VideoDecoderDelegateH264& operator=(const V4L2VideoDecoderDelegateH264&) =
      delete;

  ~V4L2VideoDecoderDelegateH264() override;

  // H264Decoder::H264Accelerator implementation.
  scoped_refptr<H264Picture> CreateH264Picture() override;
  Status SubmitFrameMetadata(const H264SPS* sps,
                             const H264PPS* pps,
                             const H264DPB& dpb,
                             const H264Picture::Vector& ref_pic_listp0,
                             const H264Picture::Vector& ref_pic_listb0,
                             const H264Picture::Vector& ref_pic_listb1,
                             scoped_refptr<H264Picture> pic) override;
  Status SubmitSlice(const H264PPS* pps,
                     const H264SliceHeader* slice_hdr,
                     const H264Picture::Vector& ref_pic_list0,
                     const H264Picture::Vector& ref_pic_list1,
                     scoped_refptr<H264Picture> pic,
                     const uint8_t* data,
                     size_t size,
                     const std::vector<SubsampleEntry>& subsamples) override;
  Status SubmitDecode(scoped_refptr<H264Picture> pic) override;
  bool OutputPicture(scoped_refptr<H264Picture> pic) override;
  void Reset() override;
  Status SetStream(base::span<const uint8_t> stream,
                   const DecryptConfig* decrypt_config,
                   uint64_t secure_handle) override;

 private:
  std::vector<scoped_refptr<V4L2DecodeSurface>> H264DPBToV4L2DPB(
      const H264DPB& dpb);
  scoped_refptr<V4L2DecodeSurface> H264PictureToV4L2DecodeSurface(
      H264Picture* pic);

  raw_ptr<V4L2DecodeSurfaceHandler> const surface_handler_;
  raw_ptr<V4L2Device> const device_;

  // Contains the kernel-specific structures that we don't want to expose
  // outside of the compilation unit.
  const std::unique_ptr<V4L2VideoDecoderDelegateH264Private> priv_;

  // Secure handle that identifies the buffer w/ the decrypted contents in the
  // secure world. Used to resolve to the corresponding FD that references the
  // secure world memory (V4L2 will resolve that fd back to this same value).
  uint64_t secure_handle_;
};

}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VIDEO_DECODER_DELEGATE_H264_H_
