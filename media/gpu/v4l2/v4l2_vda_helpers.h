// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_V4L2_V4L2_VDA_HELPERS_H_
#define MEDIA_GPU_V4L2_V4L2_VDA_HELPERS_H_

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/video_codecs.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/chromeos/image_processor.h"
#include "media/media_buildflags.h"
#include "ui/gfx/geometry/size.h"

namespace media {

class V4L2Device;
class H264Parser;
#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
class H265Parser;
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

// Helper static methods to be shared between V4L2VideoDecodeAccelerator and
// V4L2SliceVideoDecodeAccelerator. This avoids some code duplication between
// these very similar classes.
// Note: this namespace can be removed once the V4L2VDA is deprecated.
namespace v4l2_vda_helpers {

// Returns a usable input format of image processor, or nullopt if not found.
std::optional<Fourcc> FindImageProcessorInputFormat(V4L2Device* vda_device);
// Return a usable output format of image processor, or nullopt if not found.
std::optional<Fourcc> FindImageProcessorOutputFormat(V4L2Device* ip_device);

// Create and return an image processor for the given parameters, or nullptr
// if it cannot be created.
//
// |vda_output_format| is the output format of the VDA, i.e. the IP's input
// format.
// |ip_output_format| is the output format that the IP must produce.
// |vda_output_coded_size| is the coded size of the VDA output buffers (i.e.
// the input coded size for the IP).
// |ip_output_coded_size| is the coded size of the output buffers that the IP
// must produce.
// |visible_rect| is the visible area of both the input and output buffers.
// |output_storage_type| indicates what type of VideoFrame is used for output.
// |nb_buffers| is the exact number of output buffers that the IP must create.
// |image_processor_output_mode| specifies whether the IP must allocate its
// own buffers or rely on imported ones.
// |client_task_runner| is the task runner for interacting with image processor.
// |error_cb| is the error callback passed to
// V4L2ImageProcessorBackend::Create().
std::unique_ptr<ImageProcessor> CreateImageProcessor(
    const Fourcc vda_output_format,
    const Fourcc ip_output_format,
    const gfx::Size& vda_output_coded_size,
    const gfx::Size& ip_output_coded_size,
    const gfx::Rect& visible_rect,
    VideoFrame::StorageType output_storage_type,
    size_t nb_buffers,
    scoped_refptr<V4L2Device> image_processor_device,
    ImageProcessor::OutputMode image_processor_output_mode,
    scoped_refptr<base::SequencedTaskRunner> client_task_runner,
    ImageProcessor::ErrorCB error_cb);

// When importing a buffer (ARC++ use-case), the buffer's actual size may
// be different from the requested one. However, the actual size is never
// provided to us - so we need to compute it from the NativePixmapHandle.
// Given the |handle| and |fourcc| of the buffer, adjust |current_size| to
// the actual computed size of the buffer and return the new size.
gfx::Size NativePixmapSizeFromHandle(const gfx::NativePixmapHandle& handle,
                                     const Fourcc fourcc,
                                     const gfx::Size& current_size);

// Interface to split an input stream into chunks containing whole frames.
// This default implementation can be used for codecs that do not support frame
// splitting (like VP8 or VP9), whereas codecs that use slices can inherit
// and specialize it.
class InputBufferFragmentSplitter {
 public:
  static std::unique_ptr<InputBufferFragmentSplitter> CreateFromProfile(
      media::VideoCodecProfile profile);

  explicit InputBufferFragmentSplitter() = default;
  virtual ~InputBufferFragmentSplitter() = default;

  // Advance to the next fragment that begins a frame.
  virtual bool AdvanceFrameFragment(const uint8_t* data,
                                    size_t size,
                                    size_t* endpos);

  virtual void Reset();

  // Returns true if we may currently be in the middle of a frame (e.g. we
  // haven't yet parsed all the slices of a multi-slice H.264 frame).
  virtual bool IsPartialFramePending() const;
};

// Splitter for H.264, making sure to properly report when a partial frame
// may be pending.
class H264InputBufferFragmentSplitter : public InputBufferFragmentSplitter {
 public:
  explicit H264InputBufferFragmentSplitter();
  ~H264InputBufferFragmentSplitter() override;

  bool AdvanceFrameFragment(const uint8_t* data,
                            size_t size,
                            size_t* endpos) override;
  void Reset() override;
  bool IsPartialFramePending() const override;

 private:
  // For H264 decode, hardware requires that we send it frame-sized chunks.
  // We'll need to parse the stream.
  std::unique_ptr<H264Parser> h264_parser_;
  // Set if we have a pending incomplete frame in the input buffer.
  bool partial_frame_pending_ = false;
};

#if BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)
// Splitter for HEVC, making sure to properly report when a partial frame
// may be pending.
class HEVCInputBufferFragmentSplitter : public InputBufferFragmentSplitter {
 public:
  explicit HEVCInputBufferFragmentSplitter();
  ~HEVCInputBufferFragmentSplitter() override;

  bool AdvanceFrameFragment(const uint8_t* data,
                            size_t size,
                            size_t* endpos) override;
  void Reset() override;
  bool IsPartialFramePending() const override;

 private:
  // For HEVC decode, hardware requires that we send it frame-sized chunks.
  // We'll need to parse the stream.
  std::unique_ptr<H265Parser> h265_parser_;
  // Set if we have a pending incomplete frame in the input buffer.
  bool partial_frame_pending_ = false;
  // Set if we have pending slice data in the input buffer.
  bool slice_data_pending_ = false;
};
#endif  // BUILDFLAG(ENABLE_HEVC_PARSER_AND_HW_DECODER)

}  // namespace v4l2_vda_helpers
}  // namespace media

#endif  // MEDIA_GPU_V4L2_V4L2_VDA_HELPERS_H_
