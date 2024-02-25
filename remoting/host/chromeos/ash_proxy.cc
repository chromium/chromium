// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/ash_proxy.h"

#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "components/prefs/pref_service.h"
#include "components/viz/host/host_frame_sink_manager.h"
#include "remoting/base/constants.h"
#include "remoting/host/chromeos/features.h"
#include "ui/aura/env.h"
#include "ui/aura/scoped_window_capture_request.h"
#include "ui/compositor/compositor.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/screen.h"
#include "ui/display/types/display_constants.h"

namespace remoting {

namespace {

class DefaultAshProxy : public AshProxy {
 public:
  DefaultAshProxy() = default;
  DefaultAshProxy(const DefaultAshProxy&) = delete;
  DefaultAshProxy& operator=(const DefaultAshProxy&) = delete;
  ~DefaultAshProxy() override = default;

  // AshProxy implementation:
  DisplayId GetPrimaryDisplayId() const override {
    if (!screen()) {
      return display::kDefaultDisplayId;
    }

    return screen()->GetPrimaryDisplay().id();
  }

  const std::vector<display::Display>& GetActiveDisplays() const override {
    return display_manager().active_display_list();
  }

  const display::Display* GetDisplayForId(DisplayId display_id) const override {
    if (!display_manager().IsActiveDisplayId(display_id)) {
      return nullptr;
    }

    return &display_manager().GetDisplayForId(display_id);
  }

  aura::Window* GetSelectFileContainer() override {
    return shell().GetPrimaryRootWindow()->GetChildById(
        ash::kShellWindowId_AlwaysOnTopContainer);
  }

  void CreateVideoCapturer(
      mojo::PendingReceiver<viz::mojom::FrameSinkVideoCapturer> video_capturer)
      override {
    aura::Env::GetInstance()
        ->context_factory()
        ->GetHostFrameSinkManager()
        ->CreateVideoCapturer(std::move(video_capturer));
  }

  aura::ScopedWindowCaptureRequest MakeDisplayCapturable(
      DisplayId source_display_id) override {
    aura::Window* window = GetWindowToCaptureForId(source_display_id);
    DCHECK(window) << "No window exists for the source_display_id: "
                   << source_display_id;

    if (window->IsRootWindow()) {
      // Root window is always capturable so nothing to do here.
      return aura::ScopedWindowCaptureRequest();
    } else {
      return window->MakeWindowCapturable();
    }
  }

  viz::FrameSinkId GetFrameSinkId(DisplayId source_display_id) override {
    aura::Window* window =
        ash::Shell::GetRootWindowForDisplayId(source_display_id);

    DCHECK(window) << "No window exists for the source_display_id: "
                   << source_display_id;
    return window->GetFrameSinkId();
  }

  ash::curtain::SecurityCurtainController& GetSecurityCurtainController()
      override {
    return shell().security_curtain_controller();
  }

  void RequestSignOut() override {
    shell().session_controller()->RequestSignOut();
  }

  bool IsScreenReaderEnabled() const override {
    return shell().session_controller()->GetActivePrefService()->GetBoolean(
        ash::prefs::kAccessibilitySpokenFeedbackEnabled);
  }

 private:
  const display::Screen* screen() const { return display::Screen::GetScreen(); }
  // We can not return a const reference, as the ash shell has no const getter
  // for the display manager :/
  ash::Shell& shell() const {
    auto* shell = ash::Shell::Get();
    DCHECK(shell);
    return *shell;
  }
  const display::DisplayManager& display_manager() const {
    const auto* result = shell().display_manager();
    DCHECK(result);
    return *result;
  }
  aura::Window* GetRootWindowForId(DisplayId id) {
    return shell().GetRootWindowForDisplayId(id);
  }

  aura::Window* GetWindowToCaptureForId(DisplayId id) {
    aura::Window* root_window = GetRootWindowForId(id);
    if (base::FeatureList::IsEnabled(
            remoting::features::kEnableCrdAdminRemoteAccess)) {
      // Capture the uncurtained window.
      return ash::Shell::GetContainer(
          root_window, ash::kShellWindowId_ScreenAnimationContainer);
    }

    return root_window;
  }
};

AshProxy* g_instance_for_testing_ = nullptr;

}  // namespace

// static
AshProxy& AshProxy::Get() {
  static base::NoDestructor<DefaultAshProxy> instance_;

  if (g_instance_for_testing_) {
    return *g_instance_for_testing_;
  }

  return *instance_;
}

// static
void AshProxy::SetInstanceForTesting(AshProxy* instance) {
  if (instance) {
    DCHECK(!g_instance_for_testing_);
  }
  g_instance_for_testing_ = instance;
}

// static
int AshProxy::ScaleFactorToDpi(float scale_factor) {
  return static_cast<int>(scale_factor * kDefaultDpi);
}

AshProxy::~AshProxy() = default;

}  // namespace remoting
