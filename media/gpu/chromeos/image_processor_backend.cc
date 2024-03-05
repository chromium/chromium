// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/chromeos/image_processor_backend.h"

#include <memory>
#include <ostream>
#include <sstream>

#include "base/functional/bind.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "media/gpu/chromeos/video_frame_resource.h"
#include "media/gpu/macros.h"

namespace media {

namespace {

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

// Used to adapt FrameResourceReadyCB to FrameReadyCB. The incoming
// FrameResource gets to converted to VideoFrame and passed to |callback|.
void FrameResourceToFrameReadyCB(ImageProcessorBackend::FrameReadyCB callback,
                                 scoped_refptr<FrameResource> frame) {
  VideoFrameResource* video_frame_resource = frame->AsVideoFrameResource();
  // This callback only gets called when |frame| is a VideoFrameResource.
  CHECK(!!video_frame_resource);
  std::move(callback).Run(video_frame_resource->GetMutableVideoFrame());
}

}  // namespace

ImageProcessorBackend::PortConfig::PortConfig(const PortConfig&) = default;

ImageProcessorBackend::PortConfig::PortConfig(
    Fourcc fourcc,
    const gfx::Size& size,
    const std::vector<ColorPlaneLayout>& planes,
    const gfx::Rect& visible_rect,
    const std::vector<VideoFrame::StorageType>& preferred_storage_types)
    : fourcc(fourcc),
      size(size),
      planes(planes),
      visible_rect(visible_rect),
      preferred_storage_types(preferred_storage_types) {}

ImageProcessorBackend::PortConfig::~PortConfig() = default;

std::string ImageProcessorBackend::PortConfig::ToString() const {
  return base::StringPrintf(
      "PortConfig(format:%s, size:%s, planes: %s, visible_rect:%s, "
      "storage_types:%s)",
      fourcc.ToString().c_str(), size.ToString().c_str(),
      VectorToString(planes).c_str(), visible_rect.ToString().c_str(),
      VectorToString(preferred_storage_types).c_str());
}

ImageProcessorBackend::ImageProcessorBackend(
    const PortConfig& input_config,
    const PortConfig& output_config,
    OutputMode output_mode,
    ErrorCB error_cb,
    scoped_refptr<base::SequencedTaskRunner> backend_task_runner)
    : input_config_(input_config),
      output_config_(output_config),
      output_mode_(output_mode),
      error_cb_(error_cb),
      backend_task_runner_(std::move(backend_task_runner)) {
  DETACH_FROM_SEQUENCE(backend_sequence_checker_);
}

ImageProcessorBackend::~ImageProcessorBackend() = default;

void ImageProcessorBackend::Destroy() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  delete this;
}

void ImageProcessorBackend::Process(scoped_refptr<VideoFrame> input_frame,
                                    scoped_refptr<VideoFrame> output_frame,
                                    FrameReadyCB cb) {
  // Wraps ProcessFrame.
  DVLOGF(4);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  ProcessFrame(VideoFrameResource::Create(std::move(input_frame)),
               VideoFrameResource::Create(std::move(output_frame)),
               base::BindOnce(&FrameResourceToFrameReadyCB, std::move(cb)));
}

void ImageProcessorBackend::ProcessLegacyFrame(
    scoped_refptr<FrameResource> frame,
    LegacyFrameResourceReadyCB cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);

  NOTIMPLEMENTED();
}

void ImageProcessorBackend::Reset() {
  DVLOGF(3);
  DCHECK_CALLED_ON_VALID_SEQUENCE(backend_sequence_checker_);
  // Do nothing as the default action.
}

bool ImageProcessorBackend::needs_linear_output_buffers() const {
  return false;
}

bool ImageProcessorBackend::supports_incoherent_buffers() const {
  return false;
}

}  // namespace media

namespace std {

void default_delete<media::ImageProcessorBackend>::operator()(
    media::ImageProcessorBackend* ptr) const {
  ptr->Destroy();
}

}  // namespace std
