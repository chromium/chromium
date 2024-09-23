// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "remoting/protocol/fake_desktop_capturer.h"

#include <stdint.h>

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace remoting::protocol {

// FakeDesktopCapturer generates a white picture of size kWidth x kHeight
// with a rectangle of size kBoxWidth x kBoxHeight. The rectangle moves kSpeed
// pixels per frame along both axes, and bounces off the sides of the screen.
static const int kWidth = FakeDesktopCapturer::kWidth;
static const int kHeight = FakeDesktopCapturer::kHeight;
static const int kBoxWidth = 140;
static const int kBoxHeight = 140;
static const int kSpeed = 20;

static_assert(kBoxWidth < kWidth && kBoxHeight < kHeight, "bad box size");
static_assert((kBoxWidth % kSpeed == 0) && (kWidth % kSpeed == 0) &&
                  (kBoxHeight % kSpeed == 0) && (kHeight % kSpeed == 0),
              "sizes must be multiple of kSpeed");

namespace {

class DefaultFrameGenerator
    : public base::RefCountedThreadSafe<DefaultFrameGenerator> {
 public:
  DefaultFrameGenerator()
      : box_pos_x_(0),
        box_pos_y_(0),
        box_speed_x_(kSpeed),
        box_speed_y_(kSpeed),
        first_frame_(true) {}

  DefaultFrameGenerator(const DefaultFrameGenerator&) = delete;
  DefaultFrameGenerator& operator=(const DefaultFrameGenerator&) = delete;

  std::unique_ptr<webrtc::DesktopFrame> GenerateFrame(
      webrtc::SharedMemoryFactory* shared_memory_factory);

 private:
  friend class base::RefCountedThreadSafe<DefaultFrameGenerator>;
  ~DefaultFrameGenerator() = default;

  webrtc::DesktopSize size_;
  int box_pos_x_;
  int box_pos_y_;
  int box_speed_x_;
  int box_speed_y_;
  bool first_frame_;
};

std::unique_ptr<webrtc::DesktopFrame> DefaultFrameGenerator::GenerateFrame(
    webrtc::SharedMemoryFactory* shared_memory_factory) {
  const int kBytesPerPixel = webrtc::DesktopFrame::kBytesPerPixel;
  std::unique_ptr<webrtc::DesktopFrame> frame;
  if (shared_memory_factory) {
    int buffer_size = kWidth * kHeight * kBytesPerPixel;
    frame = std::make_unique<webrtc::SharedMemoryDesktopFrame>(
        webrtc::DesktopSize(kWidth, kHeight), kWidth * kBytesPerPixel,
        shared_memory_factory->CreateSharedMemory(buffer_size).release());
  } else {
    frame = std::make_unique<webrtc::BasicDesktopFrame>(
        webrtc::DesktopSize(kWidth, kHeight));
  }

  // Move the box.
  bool old_box_pos_x = box_pos_x_;
  box_pos_x_ += box_speed_x_;
  if (box_pos_x_ + kBoxWidth >= kWidth || box_pos_x_ == 0) {
    box_speed_x_ = -box_speed_x_;
  }

  bool old_box_pos_y = box_pos_y_;
  box_pos_y_ += box_speed_y_;
  if (box_pos_y_ + kBoxHeight >= kHeight || box_pos_y_ == 0) {
    box_speed_y_ = -box_speed_y_;
  }

  memset(frame->data(), 0xff, kHeight * frame->stride());

  // Draw rectangle with the following colors in its corners:
  //     cyan....yellow
  //     ..............
  //     blue.......red
  uint8_t* row = frame->data() +
                 (box_pos_y_ * size_.width() + box_pos_x_) * kBytesPerPixel;
  for (int y = 0; y < kBoxHeight; ++y) {
    for (int x = 0; x < kBoxWidth; ++x) {
      int r = x * 255 / kBoxWidth;
      int g = y * 255 / kBoxHeight;
      int b = 255 - (x * 255 / kBoxWidth);
      row[x * kBytesPerPixel] = r;
      row[x * kBytesPerPixel + 1] = g;
      row[x * kBytesPerPixel + 2] = b;
      row[x * kBytesPerPixel + 3] = 0xff;
    }
    row += frame->stride();
  }

  if (first_frame_) {
    frame->mutable_updated_region()->SetRect(
        webrtc::DesktopRect::MakeXYWH(0, 0, kWidth, kHeight));
    first_frame_ = false;
  } else {
    frame->mutable_updated_region()->SetRect(webrtc::DesktopRect::MakeXYWH(
        old_box_pos_x, old_box_pos_y, kBoxWidth, kBoxHeight));
    frame->mutable_updated_region()->AddRect(webrtc::DesktopRect::MakeXYWH(
        box_pos_x_, box_pos_y_, kBoxWidth, kBoxHeight));
  }

  return frame;
}

}  // namespace

FakeDesktopCapturer::FakeDesktopCapturer() : callback_(nullptr) {
  frame_generator_ =
      base::BindRepeating(&DefaultFrameGenerator::GenerateFrame,
                          base::MakeRefCounted<DefaultFrameGenerator>());
}

FakeDesktopCapturer::~FakeDesktopCapturer() = default;

void FakeDesktopCapturer::set_frame_generator(FrameGenerator frame_generator) {
  DCHECK(!callback_);
  frame_generator_ = std::move(frame_generator);
}

void FakeDesktopCapturer::Start(Callback* callback) {
  DCHECK(!callback_);
  DCHECK(callback);
  callback_ = callback;
}

void FakeDesktopCapturer::SetSharedMemoryFactory(
    std::unique_ptr<webrtc::SharedMemoryFactory> shared_memory_factory) {
  shared_memory_factory_ = std::move(shared_memory_factory);
}

void FakeDesktopCapturer::CaptureFrame() {
  base::Time capture_start_time = base::Time::Now();
  std::unique_ptr<webrtc::DesktopFrame> frame =
      frame_generator_.Run(shared_memory_factory_.get());
  if (frame) {
    frame->set_capture_time_ms(
        (base::Time::Now() - capture_start_time).InMillisecondsRoundedUp());
  }
  auto result = frame ? webrtc::DesktopCapturer::Result::SUCCESS
                      : webrtc::DesktopCapturer::Result::ERROR_TEMPORARY;
  // Post a task for the OnCaptureResult call to allow the stack to unwind and
  // simulate the actual product more accurately. Calling OnCaptureResult()
  // directly also leads to issues when testing with shared memory regions and
  // IPC as the callback invocation will occur before the shared region can be
  // set up.
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&webrtc::DesktopCapturer::Callback::OnCaptureResult,
                     base::Unretained(callback_), result, std::move(frame)));
}

bool FakeDesktopCapturer::GetSourceList(SourceList* sources) {
  NOTIMPLEMENTED();
  return false;
}

bool FakeDesktopCapturer::SelectSource(SourceId id) {
  NOTIMPLEMENTED();
  return false;
}

}  // namespace remoting::protocol
