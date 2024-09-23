// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/client/dual_buffer_frame_consumer.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

namespace {

// The implementation is mostly the same as webrtc::BasicDesktopFrame, except
// that it has an extra padding of one row at the end of the buffer. This is
// to workaround a bug in iOS' implementation of glTexSubimage2D that causes
// occasional SIGSEGV.
//
// glTexSubimage2D is supposed to only read
// kBytesPerPixel * width * (height - 1) bytes from the buffer but it seems to
// be reading more than that, which may end up reading protected memory.
//
// See details in crbug.com/778550
class PaddedDesktopFrame : public webrtc::DesktopFrame {
 public:
  explicit PaddedDesktopFrame(webrtc::DesktopSize size);

  PaddedDesktopFrame(const PaddedDesktopFrame&) = delete;
  PaddedDesktopFrame& operator=(const PaddedDesktopFrame&) = delete;

  ~PaddedDesktopFrame() override;

  // Creates a PaddedDesktopFrame that contains copy of |frame|.
  static std::unique_ptr<webrtc::DesktopFrame> CopyOf(
      const webrtc::DesktopFrame& frame);
};

PaddedDesktopFrame::PaddedDesktopFrame(webrtc::DesktopSize size)
    : DesktopFrame(
          size,
          kBytesPerPixel * size.width(),
          new uint8_t[kBytesPerPixel * size.width() * (size.height() + 1)],
          nullptr) {}

PaddedDesktopFrame::~PaddedDesktopFrame() {
  delete[] data_;
}

// static
std::unique_ptr<webrtc::DesktopFrame> PaddedDesktopFrame::CopyOf(
    const webrtc::DesktopFrame& frame) {
  std::unique_ptr<PaddedDesktopFrame> result =
      std::make_unique<PaddedDesktopFrame>(frame.size());
  for (int y = 0; y < frame.size().height(); ++y) {
    memcpy(result->data() + y * result->stride(),
           frame.data() + y * frame.stride(),
           frame.size().width() * kBytesPerPixel);
  }
  result->CopyFrameInfoFrom(frame);
  return result;
}

}  // namespace

DualBufferFrameConsumer::DualBufferFrameConsumer(
    RenderCallback callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    protocol::FrameConsumer::PixelFormat format)
    : callback_(std::move(callback)),
      task_runner_(task_runner),
      pixel_format_(format) {
  weak_ptr_ = weak_factory_.GetWeakPtr();
  thread_checker_.DetachFromThread();
}

DualBufferFrameConsumer::~DualBufferFrameConsumer() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void DualBufferFrameConsumer::RequestFullDesktopFrame() {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!buffers_[0]) {
    return;
  }
  DCHECK(buffers_[0]->size().equals(buffers_[1]->size()));
  // This creates a copy of buffers_[0] and merges area defined in
  // |buffer_1_mask_| from buffers_[1] into the copy.
  std::unique_ptr<webrtc::DesktopFrame> full_frame =
      PaddedDesktopFrame::CopyOf(*buffers_[0]);
  webrtc::DesktopRect desktop_rect =
        webrtc::DesktopRect::MakeSize(buffers_[0]->size());
  for (webrtc::DesktopRegion::Iterator i(buffer_1_mask_); !i.IsAtEnd();
       i.Advance()) {
    full_frame->CopyPixelsFrom(*buffers_[1], i.rect().top_left(),
                               i.rect());
  }
  full_frame->mutable_updated_region()->SetRect(desktop_rect);

  RunRenderCallback(std::move(full_frame), base::DoNothing());
}

std::unique_ptr<webrtc::DesktopFrame> DualBufferFrameConsumer::AllocateFrame(
    const webrtc::DesktopSize& size) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // Both buffers are reallocated whenever screen size changes.
  if (!buffers_[0] || !buffers_[0]->size().equals(size)) {
    buffers_[0] = webrtc::SharedDesktopFrame::Wrap(
        std::make_unique<PaddedDesktopFrame>(size));
    buffers_[1] = webrtc::SharedDesktopFrame::Wrap(
        std::make_unique<PaddedDesktopFrame>(size));
    buffer_1_mask_.Clear();
    current_buffer_ = 0;
  } else {
    current_buffer_ = (current_buffer_ + 1) % 2;
  }
  return buffers_[current_buffer_]->Share();
}

void DualBufferFrameConsumer::DrawFrame(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  DCHECK(thread_checker_.CalledOnValidThread());
  webrtc::SharedDesktopFrame* shared_frame =
      reinterpret_cast<webrtc::SharedDesktopFrame*> (frame.get());
  if (shared_frame->GetUnderlyingFrame() == buffers_[1]->GetUnderlyingFrame()) {
    buffer_1_mask_.AddRegion(frame->updated_region());
  } else if (shared_frame->GetUnderlyingFrame() ==
      buffers_[0]->GetUnderlyingFrame()) {
    buffer_1_mask_.Subtract(frame->updated_region());
  }
  RunRenderCallback(std::move(frame), std::move(done));
}

protocol::FrameConsumer::PixelFormat
DualBufferFrameConsumer::GetPixelFormat() {
  return pixel_format_;
}

base::WeakPtr<DualBufferFrameConsumer> DualBufferFrameConsumer::GetWeakPtr() {
  return weak_ptr_;
}

void DualBufferFrameConsumer::RunRenderCallback(
    std::unique_ptr<webrtc::DesktopFrame> frame,
    base::OnceClosure done) {
  if (!task_runner_) {
    callback_.Run(std::move(frame), std::move(done));
    return;
  }

  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          callback_, std::move(frame),
          base::BindOnce(base::IgnoreResult(&base::TaskRunner::PostTask),
                         base::SingleThreadTaskRunner::GetCurrentDefault(),
                         FROM_HERE, std::move(done))));
}

}  // namespace remoting
