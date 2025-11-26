// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/linux/fake_capture_stream.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "remoting/host/base/screen_resolution.h"
#include "remoting/host/linux/capture_stream.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_capture_types.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

FakeCaptureStream::FakeCaptureStream() = default;
FakeCaptureStream::~FakeCaptureStream() = default;

CaptureStream::CursorObserver::Subscription
FakeCaptureStream::AddCursorObserver(CursorObserver* observer) {
  cursor_observers.AddObserver(observer);
  return base::ScopedClosureRunner(
      base::BindOnce(&FakeCaptureStream::RemoveCursorObserver,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

void FakeCaptureStream::SetResolution(
    const webrtc::DesktopSize& new_resolution) {
  resolution_ = new_resolution;
}

const webrtc::DesktopSize& FakeCaptureStream::resolution() const {
  return resolution_;
}

void FakeCaptureStream::set_screen_id(webrtc::ScreenId screen_id) {
  screen_id_ = screen_id;
}

webrtc::ScreenId FakeCaptureStream::screen_id() const {
  return screen_id_;
}

base::WeakPtr<CaptureStream> FakeCaptureStream::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeCaptureStream::RemoveCursorObserver(CursorObserver* observer) {
  cursor_observers.RemoveObserver(observer);
}

// FakeCaptureStreamManager

FakeCaptureStreamManager::FakeCaptureStreamManager() = default;
FakeCaptureStreamManager::~FakeCaptureStreamManager() = default;

CaptureStreamManager::Observer::Subscription
FakeCaptureStreamManager::AddObserver(Observer* observer) {
  observers.AddObserver(observer);
  return base::ScopedClosureRunner(
      base::BindOnce(&FakeCaptureStreamManager::RemoveObserver,
                     weak_ptr_factory_.GetWeakPtr(), observer));
}

void FakeCaptureStreamManager::AddVirtualStream(
    const ScreenResolution& initial_resolution,
    AddStreamCallback callback) {
  ASSERT_TRUE(next_screen_id.has_value());
  auto stream =
      AddVirtualStream(*next_screen_id, initial_resolution.dimensions());
  std::move(callback).Run(base::ok(stream));
  next_screen_id.reset();
}

base::WeakPtr<CaptureStream> FakeCaptureStreamManager::AddVirtualStream(
    webrtc::ScreenId screen_id,
    const webrtc::DesktopSize& resolution) {
  auto stream = std::make_unique<FakeCaptureStream>();
  stream->set_screen_id(screen_id);
  stream->SetResolution(resolution);
  auto weak_stream = stream->GetWeakPtr();
  streams_[screen_id] = std::move(stream);
  observers.Notify(&Observer::OnPipewireCaptureStreamAdded, weak_stream);
  return weak_stream;
}

void FakeCaptureStreamManager::RemoveVirtualStream(webrtc::ScreenId screen_id) {
  streams_.erase(screen_id);
  observers.Notify(&Observer::OnPipewireCaptureStreamRemoved, screen_id);
}

base::WeakPtr<CaptureStream> FakeCaptureStreamManager::GetStream(
    webrtc::ScreenId screen_id) {
  FakeCaptureStream* fake_stream = GetFakeStream(screen_id);
  return fake_stream ? fake_stream->GetWeakPtr() : nullptr;
}

FakeCaptureStream* FakeCaptureStreamManager::GetFakeStream(
    webrtc::ScreenId screen_id) {
  const auto& it = streams_.find(screen_id);
  if (it == streams_.end()) {
    return nullptr;
  }
  return it->second.get();
}

base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>>
FakeCaptureStreamManager::GetActiveStreams() {
  base::flat_map<webrtc::ScreenId, base::WeakPtr<CaptureStream>> output;
  for (auto& [screen_id, stream] : streams_) {
    output[screen_id] = stream->GetWeakPtr();
  }
  return output;
}

base::WeakPtr<CaptureStreamManager> FakeCaptureStreamManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void FakeCaptureStreamManager::RemoveObserver(Observer* observer) {
  observers.RemoveObserver(observer);
}

}  // namespace remoting
