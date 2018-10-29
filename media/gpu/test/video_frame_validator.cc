// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/test/video_frame_validator.h"

#include <libyuv.h>

#include "base/files/file.h"
#include "base/md5.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"

namespace media {
namespace test {

// static
std::unique_ptr<VideoFrameValidator>
VideoFrameValidator::CreateVideoFrameValidator(
    const base::FilePath& prefix_output_yuv,
    const base::FilePath& md5_file_path) {
  auto video_frame_mapper = VideoFrameMapperFactory::CreateMapper();
  if (!video_frame_mapper) {
    LOG(ERROR) << "Failed to create VideoFrameMapper.";
    return nullptr;
  }
  auto md5_of_frames = ReadGoldenThumbnailMD5s(md5_file_path);
  if (md5_of_frames.empty()) {
    LOG(ERROR) << "Failed to read md5 values in " << md5_file_path;
    return nullptr;
  }
  return base::WrapUnique(
      new VideoFrameValidator(prefix_output_yuv, std::move(md5_of_frames),
                              std::move(video_frame_mapper)));
}

VideoFrameValidator::VideoFrameValidator(
    const base::FilePath& prefix_output_yuv,
    std::vector<std::string> md5_of_frames,
    std::unique_ptr<VideoFrameMapper> video_frame_mapper)
    : prefix_output_yuv_(prefix_output_yuv),
      md5_of_frames_(std::move(md5_of_frames)),
      video_frame_mapper_(std::move(video_frame_mapper)) {
  DETACH_FROM_THREAD(thread_checker_);
}

VideoFrameValidator::~VideoFrameValidator() {}

void VideoFrameValidator::EvaluateVideoFrame(
    scoped_refptr<VideoFrame> video_frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  std::string expected_md5;
  LOG_IF(FATAL, frame_index_ >= md5_of_frames_.size())
      << "Frame number is over than the number of read md5 values in file.";
  expected_md5 = md5_of_frames_[frame_index_];

  std::string computed_md5 = ComputeMD5FromVideoFrame(std::move(video_frame));
  if (computed_md5 != expected_md5) {
    mismatched_frames_.push_back(
        MismatchedFrameInfo{frame_index_, computed_md5, expected_md5});
  }
  frame_index_++;
}

std::vector<VideoFrameValidator::MismatchedFrameInfo>
VideoFrameValidator::GetMismatchedFramesInfo() const {
  return mismatched_frames_;
}

std::string VideoFrameValidator::ComputeMD5FromVideoFrame(
    scoped_refptr<VideoFrame> video_frame) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto mapped_frame = video_frame_mapper_->Map(std::move(video_frame));
  if (!mapped_frame) {
    LOG(FATAL) << "Failed to map decoded picture.";
    return std::string();
  }

  auto i420_frame = CreateI420Frame(mapped_frame.get());
  if (!i420_frame) {
    LOG(ERROR) << "Failed to convert to I420";
    return std::string();
  }

  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, i420_frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  auto md5_string = MD5DigestToBase16(digest);

  if (!prefix_output_yuv_.empty()) {
    // Output yuv.
    LOG_IF(WARNING, !WriteI420ToFile(frame_index_, i420_frame.get()))
        << "Failed to write yuv into file.";
  }
  return md5_string;
}

scoped_refptr<VideoFrame> VideoFrameValidator::CreateI420Frame(
    const VideoFrame* const src_frame) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  const auto& visible_rect = src_frame->visible_rect();
  const int width = visible_rect.width();
  const int height = visible_rect.height();
  auto dst_frame = VideoFrame::CreateFrame(
      PIXEL_FORMAT_I420, visible_rect.size(), visible_rect, visible_rect.size(),
      base::TimeDelta());
  uint8_t* const dst_y = dst_frame->data(VideoFrame::kYPlane);
  uint8_t* const dst_u = dst_frame->data(VideoFrame::kUPlane);
  uint8_t* const dst_v = dst_frame->data(VideoFrame::kVPlane);
  const int dst_stride_y = dst_frame->stride(VideoFrame::kYPlane);
  const int dst_stride_u = dst_frame->stride(VideoFrame::kUPlane);
  const int dst_stride_v = dst_frame->stride(VideoFrame::kVPlane);
  switch (src_frame->format()) {
    case PIXEL_FORMAT_NV12:
      libyuv::NV12ToI420(src_frame->data(VideoFrame::kYPlane),
                         src_frame->stride(VideoFrame::kYPlane),
                         src_frame->data(VideoFrame::kUVPlane),
                         src_frame->stride(VideoFrame::kUVPlane), dst_y,
                         dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                         width, height);
      break;
    case PIXEL_FORMAT_YV12:
    case PIXEL_FORMAT_I420:
      libyuv::I420Copy(src_frame->data(VideoFrame::kYPlane),
                       src_frame->stride(VideoFrame::kYPlane),
                       src_frame->data(VideoFrame::kUPlane),
                       src_frame->stride(VideoFrame::kUPlane),
                       src_frame->data(VideoFrame::kVPlane),
                       src_frame->stride(VideoFrame::kVPlane), dst_y,
                       dst_stride_y, dst_u, dst_stride_u, dst_v, dst_stride_v,
                       width, height);
      break;
    default:
      LOG(FATAL) << "Unsupported format: " << src_frame->format();
      return nullptr;
  }
  return dst_frame;
}

bool VideoFrameValidator::WriteI420ToFile(
    size_t frame_index,
    const VideoFrame* const video_frame) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (video_frame->format() != PIXEL_FORMAT_I420) {
    LOG(ERROR) << "No I420 format frame.";
    return false;
  }
  if (video_frame->storage_type() !=
      VideoFrame::StorageType::STORAGE_OWNED_MEMORY) {
    LOG(ERROR) << "Video frame doesn't own memory.";
    return false;
  }
  const int width = video_frame->visible_rect().width();
  const int height = video_frame->visible_rect().height();
  base::FilePath::StringType output_yuv_fname;
  base::SStringPrintf(&output_yuv_fname,
                      FILE_PATH_LITERAL("%04zu_%dx%d_I420.yuv"), frame_index,
                      width, height);
  base::File yuv_file(prefix_output_yuv_.AddExtension(output_yuv_fname),
                      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_APPEND);
  const size_t num_planes = VideoFrame::NumPlanes(video_frame->format());
  for (size_t i = 0; i < num_planes; i++) {
    size_t plane_w = VideoFrame::Rows(i, video_frame->format(), width);
    size_t plane_h = VideoFrame::Rows(i, video_frame->format(), height);
    int data_size = base::checked_cast<int>(plane_w * plane_h);
    const uint8_t* data = video_frame->data(i);
    if (yuv_file.Write(0, reinterpret_cast<const char*>(data), data_size) !=
        data_size) {
      LOG(ERROR) << "Fail to write file in plane #" << i;
      return false;
    }
  }
  return true;
}

}  // namespace test
}  // namespace media
