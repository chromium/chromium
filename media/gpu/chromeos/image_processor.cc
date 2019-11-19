// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor.h"

#include <ostream>
#include <sstream>

#include "base/strings/stringprintf.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/video_frame.h"

namespace media {

namespace {

std::ostream& operator<<(std::ostream& ostream,
                         const VideoFrame::StorageType& storage_type) {
  ostream << VideoFrame::StorageTypeToString(storage_type);
  return ostream;
}

template <class T>
std::string VectorToString(const std::vector<T>& vec) {
  std::ostringstream result;
  std::string delim;
  result << "[";
  for (const T& v : vec) {
    result << delim << v;
    if (delim.size() == 0)
      delim = ", ";
  }
  result << "]";
  return result.str();
}

}  // namespace

ImageProcessor::PortConfig::PortConfig(const PortConfig&) = default;

ImageProcessor::PortConfig::PortConfig(
    Fourcc fourcc,
    const gfx::Size& size,
    const std::vector<ColorPlaneLayout>& planes,
    const gfx::Size& visible_size,
    const std::vector<VideoFrame::StorageType>& preferred_storage_types)
    : fourcc(fourcc),
      size(size),
      planes(planes),
      visible_size(visible_size),
      preferred_storage_types(preferred_storage_types) {}

ImageProcessor::PortConfig::~PortConfig() = default;

std::string ImageProcessor::PortConfig::ToString() const {
  return base::StringPrintf(
      "PortConfig(format:%s, size:%s, planes: %s, visible_size:%s, "
      "storage_types:%s)",
      fourcc.ToString().c_str(), size.ToString().c_str(),
      VectorToString(planes).c_str(), visible_size.ToString().c_str(),
      VectorToString(preferred_storage_types).c_str());
}

ImageProcessor::ImageProcessor(const ImageProcessor::PortConfig& input_config,
                               const ImageProcessor::PortConfig& output_config,
                               OutputMode output_mode)
    : input_config_(input_config),
      output_config_(output_config),
      output_mode_(output_mode) {}

#if defined(OS_POSIX) || defined(OS_FUCHSIA)
bool ImageProcessor::Process(scoped_refptr<VideoFrame> frame,
                             LegacyFrameReadyCB cb) {
  DCHECK_EQ(output_mode(), OutputMode::ALLOCATE);

  return ProcessInternal(std::move(frame), BindToCurrentLoop(std::move(cb)));
}

bool ImageProcessor::ProcessInternal(scoped_refptr<VideoFrame> frame,
                                     LegacyFrameReadyCB cb) {
  NOTIMPLEMENTED();
  return false;
}
#endif

bool ImageProcessor::Process(scoped_refptr<VideoFrame> input_frame,
                             scoped_refptr<VideoFrame> output_frame,
                             FrameReadyCB cb) {
  DCHECK_EQ(output_mode(), OutputMode::IMPORT);

  return ProcessInternal(std::move(input_frame), std::move(output_frame),
                         BindToCurrentLoop(std::move(cb)));
}

}  // namespace media
