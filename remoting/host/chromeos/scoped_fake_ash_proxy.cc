// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/scoped_fake_ash_proxy.h"

#include "base/check.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/host/client_frame_sink_video_capturer.h"
#include "ui/aura/scoped_window_capture_request.h"
#include "ui/display/manager/managed_display_info.h"

namespace remoting::test {

ScopedFakeAshProxy::ScopedFakeAshProxy() : ScopedFakeAshProxy(nullptr) {}

ScopedFakeAshProxy::ScopedFakeAshProxy(
    ash::curtain::SecurityCurtainController* controller)
    : security_curtain_controller_(controller) {
  AshProxy::SetInstanceForTesting(this);
}

ScopedFakeAshProxy::~ScopedFakeAshProxy() {
  AshProxy::SetInstanceForTesting(nullptr);
}

display::Display& ScopedFakeAshProxy::AddPrimaryDisplayFromSpec(
    const std::string& spec,
    DisplayId id) {
  primary_display_id_ = id;
  return AddDisplayFromSpecWithId(spec, id);
}

display::Display& ScopedFakeAshProxy::AddPrimaryDisplay(DisplayId id) {
  return AddPrimaryDisplayFromSpec("800x600", id);
}

display::Display& ScopedFakeAshProxy::AddDisplayWithId(DisplayId id) {
  // Give the display a valid size.
  return AddDisplayFromSpecWithId("800x600", id);
}

display::Display& ScopedFakeAshProxy::AddDisplayFromSpecWithId(
    const std::string& spec,
    DisplayId id) {
  auto display_info =
      display::ManagedDisplayInfo::CreateFromSpecWithID(spec, id);

  display::Display new_display(display_info.id());

  // We use size_in_pixel() and not bounds_in_native() because size_in_pixel()
  // takes rotation into account (which is also what happens when adding a real
  // Display in the DisplayManager).
  gfx::Rect bounds_in_pixels(display_info.bounds_in_native().origin(),
                             display_info.size_in_pixel());

  float device_scale_factor = display_info.GetEffectiveDeviceScaleFactor();
  new_display.SetScaleAndBounds(device_scale_factor, bounds_in_pixels);
  new_display.set_rotation(display_info.GetActiveRotation());

  return AddDisplay(new_display);
}

display::Display& ScopedFakeAshProxy::AddDisplay(display::Display new_display) {
  displays_.push_back(new_display);
  return displays_.back();
}

void ScopedFakeAshProxy::RemoveDisplay(DisplayId id) {
  for (auto it = displays_.begin(); it != displays_.end(); it++) {
    if (it->id() == id) {
      displays_.erase(it);
      return;
    }
  }

  NOTREACHED();
}

void ScopedFakeAshProxy::UpdateDisplaySpec(DisplayId id,
                                           const std::string& spec) {
  RemoveDisplay(id);
  AddDisplayFromSpecWithId(spec, id);
}

void ScopedFakeAshProxy::UpdatePrimaryDisplaySpec(const std::string& spec) {
  UpdateDisplaySpec(GetPrimaryDisplayId(), spec);
}

DisplayId ScopedFakeAshProxy::GetPrimaryDisplayId() const {
  return primary_display_id_;
}

bool ScopedFakeAshProxy::HasPrimaryDisplay() const {
  return primary_display_id_ != -1;
}

const std::vector<display::Display>& ScopedFakeAshProxy::GetActiveDisplays()
    const {
  return displays_;
}

const display::Display* ScopedFakeAshProxy::GetDisplayForId(
    DisplayId display_id) const {
  for (const auto& display : displays_) {
    if (display_id == display.id()) {
      return &display;
    }
  }
  return nullptr;
}

aura::Window* ScopedFakeAshProxy::GetSelectFileContainer() {
  return nullptr;
}

void ScopedFakeAshProxy::CreateVideoCapturer(
    mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> video_capturer) {
  DCHECK(receiver_) << "Your test must call SetVideoCapturerReceiver() first";

  receiver_->Bind(std::move(video_capturer));
}

aura::ScopedWindowCaptureRequest ScopedFakeAshProxy::MakeDisplayCapturable(
    DisplayId source_display_id) {
  return aura::ScopedWindowCaptureRequest();
}

viz::FrameSinkId ScopedFakeAshProxy::GetFrameSinkId(
    DisplayId source_display_id) {
  return viz::FrameSinkId(source_display_id, source_display_id);
}

void ScopedFakeAshProxy::SetVideoCapturerReceiver(
    mojo::Receiver<viz::mojom::FrameSinkVideoCapturer>* receiver) {
  receiver_ = receiver;
}

ash::curtain::SecurityCurtainController&
ScopedFakeAshProxy::GetSecurityCurtainController() {
  DCHECK(security_curtain_controller_)
      << "Your test must pass a SecurityCurtainController to the constructor "
         "of ScopedFakeAshProxy before it can be used.";
  return *security_curtain_controller_;
}

void ScopedFakeAshProxy::RequestSignOut() {
  request_sign_out_count_++;
}

int ScopedFakeAshProxy::request_sign_out_count() const {
  return request_sign_out_count_;
}

bool ScopedFakeAshProxy::IsScreenReaderEnabled() const {
  return false;
}

}  // namespace remoting::test
