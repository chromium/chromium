// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_H_
#define MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "media/base/color_plane_layout.h"
#include "media/base/video_frame.h"
#include "media/gpu/chromeos/fourcc.h"
#include "media/gpu/media_gpu_export.h"
#include "ui/gfx/geometry/size.h"

namespace media {

// An image processor is used to convert from one image format to another (e.g.
// I420 to NV12) while optionally scaling. It is useful in situations where
// a given video hardware (e.g. decoder or encoder) accepts or produces data
// in a format different from what the rest of the pipeline expects.
//
// This class exposes the interface that an image processor should implement.
// The threading model of ImageProcessor:
// There are two threads, "client thread" and "processor thread".
// "client thread" is the thread that creates the ImageProcessor.
// Process(), Reset() and callbacks (i.e. FrameReadyCB and ErrorCB) must be run
// on client thread.
// ImageProcessor should have its owned thread, "processor thread", so that
// Process() doesn't block client thread. The callbacks can be called on
// processor thread. ImageProcessor's client must guarantee the callback finally
// posts them to and run on the thread that creates ImageProcessor.
class MEDIA_GPU_EXPORT ImageProcessor {
 public:
  // OutputMode is used as intermediate stage. The ultimate goal is to make
  // ImageProcessor's clients all use IMPORT output mode.
  // TODO(crbug.com/907767): Remove this once ImageProcessor always works as
  // IMPORT mode for output.
  enum class OutputMode { ALLOCATE, IMPORT };

  // Encapsulates ImageProcessor input / output configurations.
  struct MEDIA_GPU_EXPORT PortConfig {
    PortConfig() = delete;
    PortConfig(const PortConfig&);
    PortConfig(
        Fourcc fourcc,
        const gfx::Size& size,
        const std::vector<ColorPlaneLayout>& planes,
        const gfx::Size& visible_size,
        const std::vector<VideoFrame::StorageType>& preferred_storage_types);
    ~PortConfig();

    // Get the first |preferred_storage_types|.
    // If |preferred_storage_types| is empty, return STORAGE_UNKNOWN.
    VideoFrame::StorageType storage_type() const {
      return preferred_storage_types.empty() ? VideoFrame::STORAGE_UNKNOWN
                                             : preferred_storage_types.front();
    }

    // Output human readable string of PortConfig.
    // Example:
    // PortConfig(format::NV12, size:640x480, planes:[(640, 0, 307200),
    // (640,0,153600)], visible_size:640x480, storage_types:[DMABUFS])
    std::string ToString() const;

    // Video frame format represented as fourcc type.
    const Fourcc fourcc;

    // Width and height of the video frame in pixels. This must include pixel
    // data for the whole image; i.e. for YUV formats with subsampled chroma
    // planes. If a visible portion of the image does not line up on a sample
    // boundary, |size_| must be rounded up appropriately.
    const gfx::Size size;

    // Layout property (stride, offset, size of bytes) for each color plane.
    const std::vector<ColorPlaneLayout> planes;
    const gfx::Size visible_size;
    // List of preferred storage types.
    const std::vector<VideoFrame::StorageType> preferred_storage_types;
  };

  // Callback to be used to return a processed image to the client.
  // FrameReadyCB is guaranteed to be executed on the "client thread".
  using FrameReadyCB = base::OnceCallback<void(scoped_refptr<VideoFrame>)>;
  // Callback to be used to return a processed image to the client.
  // Used when calling the "legacy" Process() method with buffers that are
  // managed by the IP. The first argument is the index of the returned buffer.
  // FrameReadyCB is guaranteed to be executed on the "client thread".
  using LegacyFrameReadyCB =
      base::OnceCallback<void(size_t, scoped_refptr<VideoFrame>)>;

  // Callback to be used to notify client when ImageProcess encounters error.
  // It should be assigned in subclass' factory method. ErrorCB is guaranteed to
  // be executed on the "client thread".
  using ErrorCB = base::RepeatingClosure;

  virtual ~ImageProcessor() = default;

  const PortConfig& input_config() const { return input_config_; }
  const PortConfig& output_config() const { return output_config_; }

  // Returns output mode.
  // TODO(crbug.com/907767): Remove it once ImageProcessor always works as
  // IMPORT mode for output.
  OutputMode output_mode() const { return output_mode_; }

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  // Called by client to process |frame|. The resulting processed frame will be
  // stored in a ImageProcessor-owned output buffer and notified via |cb|. The
  // processor will drop all its references to |frame| after it finishes
  // accessing it.
  // Process() must be called on "client thread". This should not be blocking
  // function.
  //
  // Note: because base::ScopedFD is defined under OS_POXIS or OS_FUCHSIA, we
  // define this function under the same build config. It doesn't affect its
  // current users as they are all under the same build config.
  // TODO(crbug.com/907767): Remove this once ImageProcessor always works as
  // IMPORT mode for output.
  bool Process(scoped_refptr<VideoFrame> frame, LegacyFrameReadyCB cb);
#endif

  // Called by client to process |input_frame| and store in |output_frame|. This
  // can only be used when output mode is IMPORT. The processor will drop all
  // its references to |input_frame| and |output_frame| after it finishes
  // accessing it.
  // Process() must be called on "client thread". This should not be blocking
  // function.
  bool Process(scoped_refptr<VideoFrame> input_frame,
               scoped_refptr<VideoFrame> output_frame,
               FrameReadyCB cb);

  // Reset all processing frames. After this method returns, no more callbacks
  // will be invoked. ImageProcessor is ready to process more frames.
  // Reset() must be called on "client thread".
  virtual bool Reset() = 0;

 protected:
  ImageProcessor(const PortConfig& input_config,
                 const PortConfig& output_config,
                 OutputMode output_mode);

  const PortConfig input_config_;
  const PortConfig output_config_;

  // TODO(crbug.com/907767): Remove |output_mode_| once ImageProcessor always
  // works as IMPORT mode for output.
  const OutputMode output_mode_;

 private:
  // Each ImageProcessor shall implement ProcessInternal() as Process().
#if defined(OS_POSIX) || defined(OS_FUCHSIA)
  virtual bool ProcessInternal(scoped_refptr<VideoFrame> frame,
                               LegacyFrameReadyCB cb);
#endif
  virtual bool ProcessInternal(scoped_refptr<VideoFrame> input_frame,
                               scoped_refptr<VideoFrame> output_frame,
                               FrameReadyCB cb) = 0;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ImageProcessor);
};

}  // namespace media

#endif  // MEDIA_GPU_CHROMEOS_IMAGE_PROCESSOR_H_
