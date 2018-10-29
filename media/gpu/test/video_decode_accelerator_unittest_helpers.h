// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_UNITTEST_HELPERS_H_
#define MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_UNITTEST_HELPERS_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "media/base/video_codecs.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"
#include "ui/gfx/native_pixmap.h"

#if defined(OS_CHROMEOS)
namespace ui {
class OzoneGpuTestHelper;
}  // namespace ui
#endif

namespace media {
namespace test {

// Initialize the GPU thread for rendering. We only need to setup once
// for all test cases.
class VideoDecodeAcceleratorTestEnvironment : public ::testing::Environment {
 public:
  explicit VideoDecodeAcceleratorTestEnvironment(bool use_gl_renderer);

  virtual ~VideoDecodeAcceleratorTestEnvironment();

  void SetUp() override;
  void TearDown() override;
  scoped_refptr<base::SingleThreadTaskRunner> GetRenderingTaskRunner() const;

 private:
  bool use_gl_renderer_;
  base::Thread rendering_thread_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VideoDecodeAcceleratorTestEnvironment);
};

// A helper class used to manage the lifetime of a Texture. Can be backed by
// either a buffer allocated by the VDA, or by a preallocated pixmap.
class TextureRef : public base::RefCounted<TextureRef> {
 public:
  static scoped_refptr<TextureRef> Create(
      uint32_t texture_id,
      base::OnceClosure no_longer_needed_cb);

  static scoped_refptr<TextureRef> CreatePreallocated(
      uint32_t texture_id,
      base::OnceClosure no_longer_needed_cb,
      VideoPixelFormat pixel_format,
      const gfx::Size& size);

  gfx::GpuMemoryBufferHandle ExportGpuMemoryBufferHandle() const;
  scoped_refptr<VideoFrame> CreateVideoFrame(
      const gfx::Rect& visible_rect) const;

  int32_t texture_id() const { return texture_id_; }

 private:
  friend class base::RefCounted<TextureRef>;

  TextureRef(uint32_t texture_id, base::OnceClosure no_longer_needed_cb);
  ~TextureRef();

  uint32_t texture_id_;
  base::OnceClosure no_longer_needed_cb_;
#if defined(OS_CHROMEOS)
  scoped_refptr<gfx::NativePixmap> pixmap_;
  gfx::Size coded_size_;
#endif
  THREAD_CHECKER(thread_checker_);
};

class EncodedDataHelper {
 public:
  EncodedDataHelper(const std::string& encoded_data, VideoCodecProfile profile);
  ~EncodedDataHelper();

  // Compute and return the next fragment to be sent to the decoder, starting
  // from the current position in the stream, and advance the current position
  // to after the returned fragment.
  std::string GetBytesForNextData();
  static bool HasConfigInfo(const uint8_t* data,
                            size_t size,
                            VideoCodecProfile profile);

  void Rewind() { next_pos_to_decode_ = 0; }
  bool AtHeadOfStream() const { return next_pos_to_decode_ == 0; }
  bool ReachEndOfStream() const { return next_pos_to_decode_ == data_.size(); }

  size_t num_skipped_fragments() { return num_skipped_fragments_; }

 private:
  // For h.264.
  std::string GetBytesForNextFragment();
  // For VP8/9.
  std::string GetBytesForNextFrame();

  // Helpers for GetBytesForNextFragment above.
  size_t GetBytesForNextNALU(size_t pos);
  bool IsNALHeader(const std::string& data, size_t pos);
  bool LookForSPS(size_t* skipped_fragments_count);

  std::string data_;
  VideoCodecProfile profile_;
  size_t next_pos_to_decode_ = 0;
  size_t num_skipped_fragments_ = 0;
};

// Read in golden MD5s for the thumbnailed rendering of this video
std::vector<std::string> ReadGoldenThumbnailMD5s(
    const base::FilePath& md5_file_path);

// Convert from RGBA to RGB.
// Return false if any alpha channel is not 0xff, otherwise true.
bool ConvertRGBAToRGB(const std::vector<unsigned char>& rgba,
                      std::vector<unsigned char>* rgb);

}  // namespace test
}  // namespace media
#endif  // MEDIA_GPU_TEST_VIDEO_DECODE_ACCELERATOR_UNITTEST_HELPERS_H_
