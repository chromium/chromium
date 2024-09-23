// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_AV1_ACCELERATOR_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_AV1_ACCELERATOR_H_

#include <CoreMedia/CoreMedia.h>
#include <stdint.h>

#include <memory>
#include <optional>

#include "base/apple/scoped_cftyperef.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/gpu/av1_decoder.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class MediaLog;

class MEDIA_GPU_EXPORT VideoToolboxAV1Accelerator
    : public AV1Decoder::AV1Accelerator {
 public:
  using DecodeCB = base::RepeatingCallback<void(
      base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
      VideoToolboxDecompressionSessionMetadata,
      scoped_refptr<CodecPicture>)>;
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<CodecPicture>)>;

  VideoToolboxAV1Accelerator(std::unique_ptr<MediaLog> media_log,
                             std::optional<gfx::HDRMetadata> hdr_metadata,
                             DecodeCB decode_cb,
                             OutputCB output_cb);
  ~VideoToolboxAV1Accelerator() override;

  // AV1Accelerator implementation.
  scoped_refptr<AV1Picture> CreateAV1Picture(bool apply_grain) override;
  Status SubmitDecode(const AV1Picture& pic,
                      const libgav1::ObuSequenceHeader& sequence_header,
                      const AV1ReferenceFrameVector& ref_frames,
                      const libgav1::Vector<libgav1::TileBuffer>& tile_buffers,
                      base::span<const uint8_t> data) override;
  bool OutputPicture(const AV1Picture& pic) override;
  Status SetStream(base::span<const uint8_t> stream,
                   const DecryptConfig* decrypt_config) override;

 private:
  bool ProcessFormat(const AV1Picture& pic,
                     const libgav1::ObuSequenceHeader& sequence_header,
                     base::span<const uint8_t> data);

  std::unique_ptr<MediaLog> media_log_;
  std::optional<gfx::HDRMetadata> hdr_metadata_;

  // Callbacks are called synchronously, which is always re-entrant.
  DecodeCB decode_cb_;
  OutputCB output_cb_;

  // Parameters of the active format.
  VideoColorSpace active_color_space_;
  VideoCodecProfile active_profile_ = VIDEO_CODEC_PROFILE_UNKNOWN;
  std::optional<gfx::HDRMetadata> active_hdr_metadata_;
  gfx::Size active_coded_size_;

  base::apple::ScopedCFTypeRef<CMFormatDescriptionRef> active_format_;
  VideoToolboxDecompressionSessionMetadata session_metadata_;

  // Data for the current frame.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> temporal_unit_data_;
  bool format_processed_ = false;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_AV1_ACCELERATOR_H_
