// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_MAC_VIDEO_TOOLBOX_VP9_ACCELERATOR_H_
#define MEDIA_GPU_MAC_VIDEO_TOOLBOX_VP9_ACCELERATOR_H_

#include <CoreMedia/CoreMedia.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <vector>

#include "base/apple/scoped_cftyperef.h"
#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_color_space.h"
#include "media/gpu/mac/video_toolbox_decompression_metadata.h"
#include "media/gpu/media_gpu_export.h"
#include "media/gpu/vp9_decoder.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/hdr_metadata.h"

namespace media {

class MediaLog;

// The VideoToolbox VP9 decoder operates on unprocessed bitstream. This
// VP9Accelerator assembles `pic->frame_hdr->data` across multiple
// SubmitDecode()/OutputPicture() calls into superframes. Every assembled
// superframe is produces exactly one output.
class MEDIA_GPU_EXPORT VideoToolboxVP9Accelerator
    : public VP9Decoder::VP9Accelerator {
 public:
  using DecodeCB = base::RepeatingCallback<void(
      base::apple::ScopedCFTypeRef<CMSampleBufferRef>,
      VideoToolboxDecompressionSessionMetadata,
      scoped_refptr<CodecPicture>)>;
  using OutputCB = base::RepeatingCallback<void(scoped_refptr<CodecPicture>)>;

  VideoToolboxVP9Accelerator(std::unique_ptr<MediaLog> media_log,
                             std::optional<gfx::HDRMetadata> hdr_metadata,
                             DecodeCB decode_cb,
                             OutputCB output_cb);
  ~VideoToolboxVP9Accelerator() override;

  // VP9Accelerator implementation.
  scoped_refptr<VP9Picture> CreateVP9Picture() override;
  Status SubmitDecode(scoped_refptr<VP9Picture> pic,
                      const Vp9SegmentationParams& segm_params,
                      const Vp9LoopFilterParams& lf_params,
                      const Vp9ReferenceFrameVector& reference_frames) override;
  bool OutputPicture(scoped_refptr<VP9Picture> pic) override;
  bool NeedsCompressedHeaderParsed() const override;

 private:
  // Grow the current superframe.
  bool ProcessFrame(scoped_refptr<VP9Picture> pic);
  // Build a format description.
  bool ProcessFormat(scoped_refptr<VP9Picture> pic, bool* format_changed);
  // Submit the current superframe for decoding.
  bool SubmitFrames(scoped_refptr<VP9Picture> output_pic);
  // Helper to append data to a CMBlockBuffer.
  bool AppendData(CMBlockBufferRef dest, const uint8_t* data, size_t data_size);

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

  // The superframe currently being built.
  base::apple::ScopedCFTypeRef<CMBlockBufferRef> frame_data_;
  std::vector<size_t> frame_sizes_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace media

#endif  // MEDIA_GPU_MAC_VIDEO_TOOLBOX_VP9_ACCELERATOR_H_
