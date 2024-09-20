// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "media/gpu/test/video_frame_file_writer.h"

#include <utility>
#include <vector>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/video_frame_mapper.h"
#include "media/gpu/video_frame_mapper_factory.h"
#include "media/media_buildflags.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/codec/png_codec.h"

#if BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)
#include <sys/mman.h>
#endif  // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_LINUX)

namespace media {
namespace test {

VideoFrameFileWriter::VideoFrameFileWriter(
    const base::FilePath& output_folder,
    OutputFormat output_format,
    size_t output_limit,
    const base::FilePath::StringType& output_file_prefix)
    : output_folder_(output_folder),
      output_format_(output_format),
      output_limit_(output_limit),
      output_file_prefix_(output_file_prefix),
      num_frames_writing_(0),
      frame_writer_thread_("FrameWriterThread"),
      frame_writer_cv_(&frame_writer_lock_) {
  DETACH_FROM_SEQUENCE(writer_sequence_checker_);
  DETACH_FROM_SEQUENCE(writer_thread_sequence_checker_);
}

VideoFrameFileWriter::~VideoFrameFileWriter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_sequence_checker_);
  if (frame_writer_thread_.task_runner()) {
    // It's safe to use base::Unretained(this) because we own
    // |frame_writer_thread_|, so |this| should be valid until at least the
    // frame_writer_thread_.Stop() returns below which won't happen until
    // CleanUpOnWriterThread() returns.
    frame_writer_thread_.task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VideoFrameFileWriter::CleanUpOnWriterThread,
                                  base::Unretained(this)));
  }

  frame_writer_thread_.Stop();
  base::AutoLock auto_lock(frame_writer_lock_);
  DCHECK_EQ(0u, num_frames_writing_);
}

void VideoFrameFileWriter::CleanUpOnWriterThread() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);
  video_frame_mapper_.reset();
}

// static
std::unique_ptr<VideoFrameFileWriter> VideoFrameFileWriter::Create(
    const base::FilePath& output_folder,
    OutputFormat output_format,
    size_t output_limit,
    const base::FilePath::StringType& output_file_prefix) {
  // If the directory is not absolute, consider it relative to our working dir.
  base::FilePath resolved_output_folder(output_folder);
  if (!resolved_output_folder.IsAbsolute()) {
    resolved_output_folder =
        base::MakeAbsoluteFilePath(
            base::FilePath(base::FilePath::kCurrentDirectory))
            .Append(resolved_output_folder);
  }

  // Create the directory tree if it doesn't exist yet.
  if (!DirectoryExists(resolved_output_folder) &&
      !base::CreateDirectory(resolved_output_folder)) {
    LOG(ERROR) << "Failed to create a output directory: "
               << resolved_output_folder;
    return nullptr;
  }

  auto frame_file_writer = base::WrapUnique(new VideoFrameFileWriter(
      resolved_output_folder, output_format, output_limit, output_file_prefix));
  if (!frame_file_writer->Initialize()) {
    LOG(ERROR) << "Failed to initialize VideoFrameFileWriter";
    return nullptr;
  }

  return frame_file_writer;
}

bool VideoFrameFileWriter::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_sequence_checker_);
  if (!frame_writer_thread_.Start()) {
    LOG(ERROR) << "Failed to start file writer thread";
    return false;
  }

  return true;
}

void VideoFrameFileWriter::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  // Don't write more frames than the specified output limit.
  if (num_frames_writes_requested_ >= output_limit_)
    return;

  if (video_frame->visible_rect().IsEmpty()) {
    // This occurs in bitstream buffer in webrtc scenario.
    DLOG(WARNING) << "Skipping writing, frame_index=" << frame_index
                  << " because visible_rect is empty";
    return;
  }

  num_frames_writes_requested_++;

  base::AutoLock auto_lock(frame_writer_lock_);
  num_frames_writing_++;

  // Unretained is safe here, as we should not destroy the writer while there
  // are still frames being written.
  frame_writer_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&VideoFrameFileWriter::ProcessVideoFrameTask,
                     base::Unretained(this), video_frame, frame_index));
}

bool VideoFrameFileWriter::WaitUntilDone() {
  base::AutoLock auto_lock(frame_writer_lock_);
  while (num_frames_writing_ > 0) {
    frame_writer_cv_.Wait();
  }
  return true;
}

void VideoFrameFileWriter::ProcessVideoFrameTask(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  const gfx::Size& visible_size = video_frame->visible_rect().size();

  base::FilePath file_path;
  if (!output_file_prefix_.empty()) {
    file_path = base::FilePath(output_file_prefix_)
                    .AppendASCII("_")
                    .Append(base::FilePath::FromASCII(base::StringPrintf(
                        "frame_%04zu_%dx%d", frame_index, visible_size.width(),
                        visible_size.height())));
  } else {
    file_path = base::FilePath::FromASCII(
        base::StringPrintf("frame_%04zu_%dx%d", frame_index,
                           visible_size.width(), visible_size.height()));
  }

  // Copies to |frame| in this function so that |video_frame| stays alive until
  // in the end of function.
  auto frame = video_frame;
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (frame->storage_type() == VideoFrame::STORAGE_GPU_MEMORY_BUFFER) {
    // TODO(andrescj): This is a workaround. ClientNativePixmapFactoryDmabuf
    // creates ClientNativePixmapOpaque for SCANOUT_VDA_WRITE buffers which
    // does not allow us to map GpuMemoryBuffers easily for testing.
    // Therefore, we extract the dma-buf FDs. Alternatively, we could consider
    // creating our own ClientNativePixmapFactory for testing.
    frame = CreateDmabufVideoFrame(frame.get());
    if (!frame) {
      LOG(ERROR) << "Failed to create Dmabuf-backed VideoFrame from "
                 << "GpuMemoryBuffer-based VideoFrame";
      return;
    }
  }
  // Create VideoFrameMapper if not yet created. The decoder's output pixel
  // format is not known yet when creating the VideoFrameWriter. We can only
  // create the VideoFrameMapper upon receiving the first video frame.
  if ((frame->storage_type() == VideoFrame::STORAGE_DMABUFS) &&
      !video_frame_mapper_) {
    video_frame_mapper_ = VideoFrameMapperFactory::CreateMapper(
        frame->format(), frame->storage_type());
    ASSERT_TRUE(video_frame_mapper_) << "Failed to create VideoFrameMapper";
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  switch (output_format_) {
    case OutputFormat::kPNG:
      WriteVideoFramePNG(frame, file_path);
      break;
    case OutputFormat::kYUV:
      WriteVideoFrameYUV(frame, file_path);
      break;
  }

  base::AutoLock auto_lock(frame_writer_lock_);
  num_frames_writing_--;
  frame_writer_cv_.Signal();
}

void VideoFrameFileWriter::WriteVideoFramePNG(
    scoped_refptr<const VideoFrame> video_frame,
    const base::FilePath& filename) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  auto mapped_frame = video_frame;
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (video_frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    CHECK(video_frame_mapper_);
    mapped_frame = video_frame_mapper_->Map(std::move(video_frame), PROT_READ);
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  if (!mapped_frame) {
    LOG(ERROR) << "Failed to map video frame";
    return;
  }

  scoped_refptr<const VideoFrame> argb_out_frame = mapped_frame;
  if (argb_out_frame->format() != PIXEL_FORMAT_ARGB) {
    argb_out_frame = ConvertVideoFrame(argb_out_frame.get(),
                                       VideoPixelFormat::PIXEL_FORMAT_ARGB);
  }

  // Convert the ARGB frame to PNG.
  std::vector<uint8_t> png_output;
  const bool png_encode_status = gfx::PNGCodec::Encode(
      argb_out_frame->visible_data(VideoFrame::Plane::kARGB),
      gfx::PNGCodec::FORMAT_BGRA, argb_out_frame->visible_rect().size(),
      argb_out_frame->stride(VideoFrame::Plane::kARGB),
      true, /* discard_transparency */
      std::vector<gfx::PNGCodec::Comment>(), &png_output);
  ASSERT_TRUE(png_encode_status);

  // Write the PNG data to file.
  base::FilePath file_path(
      output_folder_.Append(filename).AddExtension(FILE_PATH_LITERAL(".png")));
  ASSERT_TRUE(base::WriteFile(file_path, png_output));
}

void VideoFrameFileWriter::WriteVideoFrameYUV(
    scoped_refptr<const VideoFrame> video_frame,
    const base::FilePath& filename) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(writer_thread_sequence_checker_);

  auto mapped_frame = video_frame;
#if BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)
  if (video_frame->storage_type() == VideoFrame::STORAGE_DMABUFS) {
    CHECK(video_frame_mapper_);
    mapped_frame = video_frame_mapper_->Map(std::move(video_frame), PROT_READ);
  }
#endif  // BUILDFLAG(USE_CHROMEOS_MEDIA_ACCELERATION)

  if (!mapped_frame) {
    LOG(ERROR) << "Failed to map video frame";
    return;
  }

  const VideoPixelFormat yuv_out_format =
      VideoFrame::BytesPerElement(mapped_frame->format(), 0) > 1
          ? PIXEL_FORMAT_YUV420P10
          : PIXEL_FORMAT_I420;
  scoped_refptr<const VideoFrame> out_frame = mapped_frame;
  if (out_frame->format() != PIXEL_FORMAT_I420 &&
      out_frame->format() != PIXEL_FORMAT_YUV420P10) {
    out_frame = ConvertVideoFrame(out_frame.get(), yuv_out_format);
  }

  // Write the YUV data to file.
  base::FilePath file_path(
      output_folder_.Append(filename)
          .AddExtension(FILE_PATH_LITERAL(".yuv"))
          .InsertBeforeExtension(yuv_out_format == PIXEL_FORMAT_I420
                                     ? FILE_PATH_LITERAL("_I420")
                                     : FILE_PATH_LITERAL("_I420P10")));
  base::File yuv_file(file_path,
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);

  const gfx::Size visible_size = out_frame->visible_rect().size();
  const VideoPixelFormat pixel_format = out_frame->format();
  const size_t num_planes = VideoFrame::NumPlanes(pixel_format);
  for (size_t i = 0; i < num_planes; i++) {
    const uint8_t* data = out_frame->visible_data(i);
    const int stride = out_frame->stride(i);
    const size_t rows =
        VideoFrame::Rows(i, pixel_format, visible_size.height());
    const int row_bytes =
        VideoFrame::RowBytes(i, pixel_format, visible_size.width());
    ASSERT_TRUE(stride > 0);
    for (size_t row = 0; row < rows; ++row) {
      if (yuv_file.WriteAtCurrentPos(
              reinterpret_cast<const char*>(data + (stride * row)),
              row_bytes) != row_bytes) {
        LOG(ERROR) << "Failed to write plane #" << i << " to file: "
                   << base::File::ErrorToString(base::File::GetLastFileError());
      }
    }
  }
}

}  // namespace test
}  // namespace media
