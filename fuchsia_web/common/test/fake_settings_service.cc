// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/fake_settings_service.h"

#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <lib/sys/cpp/outgoing_directory.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/memory/raw_ref.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::component_testing::ChildRef;
using ::component_testing::Protocol;
using ::component_testing::Route;

namespace test {

// Implements fuchsia.settings.Display for a single client connection.
class FakeSettingsService::DisplayImpl : public fuchsia::settings::Display {
 public:
  explicit DisplayImpl(FakeSettingsService& service) : service_(service) {}
  ~DisplayImpl() override = default;

  // Notifies the client's pending Watch callback, if any, that the theme has
  // changed.
  void NotifyThemeChanged(fuchsia::settings::ThemeType theme) {
    if (pending_watch_callback_) {
      last_reported_theme_ = theme;
      auto callback = std::move(pending_watch_callback_);
      callback(service_->MakeDisplaySettings());
    }
  }

  // fuchsia::settings::Display:
  void Watch(WatchCallback callback) override {
    if (last_reported_theme_ != service_->theme_) {
      last_reported_theme_ = service_->theme_;
      callback(service_->MakeDisplaySettings());
      return;
    }
    // Subsequent calls hang until the settings change.
    pending_watch_callback_ = std::move(callback);
  }

  void Set(fuchsia::settings::DisplaySettings settings,
           SetCallback callback) override {
    if (settings.has_theme() && settings.theme().has_theme_type()) {
      service_->set_theme(settings.theme().theme_type());
    }
    callback(fuchsia::settings::Display_Set_Result::WithResponse(
        fuchsia::settings::Display_Set_Response()));
  }

 private:
  const raw_ref<FakeSettingsService> service_;
  std::optional<fuchsia::settings::ThemeType> last_reported_theme_;
  WatchCallback pending_watch_callback_;
};

// Implements fuchsia.settings.Input for a single client connection.
class FakeSettingsService::InputImpl : public fuchsia::settings::Input {
 public:
  explicit InputImpl(FakeSettingsService& service) : service_(service) {}
  ~InputImpl() override = default;

  // Notifies the client's pending Watch callback, if any, that the microphone
  // mute state has changed.
  void NotifyInputChanged(bool muted) {
    if (pending_watch_callback_) {
      last_reported_mic_muted_ = muted;
      auto callback = std::move(pending_watch_callback_);
      callback(service_->MakeInputSettings());
    }
  }

  // fuchsia::settings::Input:
  void Watch(WatchCallback callback) override {
    if (last_reported_mic_muted_ != service_->mic_muted_) {
      last_reported_mic_muted_ = service_->mic_muted_;
      callback(service_->MakeInputSettings());
      return;
    }
    // Subsequent calls hang until the settings change.
    pending_watch_callback_ = std::move(callback);
  }

  void Set(std::vector<fuchsia::settings::InputState> input_states,
           SetCallback callback) override {
    callback(fuchsia::settings::Input_Set_Result::WithResponse(
        fuchsia::settings::Input_Set_Response()));
  }

 private:
  const raw_ref<FakeSettingsService> service_;
  std::optional<bool> last_reported_mic_muted_;
  WatchCallback pending_watch_callback_;
};

FakeSettingsService::FakeSettingsService() = default;

FakeSettingsService::~FakeSettingsService() = default;

void FakeSettingsService::RouteToChild(
    ::component_testing::RealmBuilder& realm_builder,
    std::string_view child_name,
    std::unique_ptr<FakeSettingsService> service) {
  static constexpr char kSettingsServiceName[] = "fake_settings";
  if (service) {
    realm_builder.AddLocalChild(kSettingsServiceName,
                                [service = std::move(service)]() mutable {
                                  return std::move(service);
                                });
  } else {
    realm_builder.AddLocalChild(kSettingsServiceName, []() {
      return std::make_unique<FakeSettingsService>();
    });
  }
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{fuchsia::settings::Display::Name_},
                             Protocol{fuchsia::settings::Input::Name_}},
            .source = ChildRef{kSettingsServiceName},
            .targets = {ChildRef{child_name}}});
}

void FakeSettingsService::set_theme(fuchsia::settings::ThemeType theme) {
  if (theme_ == theme) {
    return;
  }
  theme_ = theme;
  for (const auto& binding : display_bindings_.bindings()) {
    binding->impl()->NotifyThemeChanged(theme_);
  }
}

void FakeSettingsService::set_mic_muted(bool muted) {
  if (mic_muted_ == muted) {
    return;
  }
  mic_muted_ = muted;
  for (const auto& binding : input_bindings_.bindings()) {
    binding->impl()->NotifyInputChanged(mic_muted_);
  }
}

void FakeSettingsService::OnStart() {
  ASSERT_EQ(
      outgoing()->AddPublicService<fuchsia::settings::Display>(
          [this](fidl::InterfaceRequest<fuchsia::settings::Display> request) {
            display_bindings_.AddBinding(std::make_unique<DisplayImpl>(*this),
                                         std::move(request));
          }),
      ZX_OK);
  ASSERT_EQ(
      outgoing()->AddPublicService<fuchsia::settings::Input>(
          [this](fidl::InterfaceRequest<fuchsia::settings::Input> request) {
            input_bindings_.AddBinding(std::make_unique<InputImpl>(*this),
                                       std::move(request));
          }),
      ZX_OK);
}

fuchsia::settings::DisplaySettings FakeSettingsService::MakeDisplaySettings()
    const {
  fuchsia::settings::DisplaySettings settings;
  fuchsia::settings::Theme theme;
  theme.set_theme_type(theme_);
  settings.set_theme(std::move(theme));
  return settings;
}

fuchsia::settings::InputSettings FakeSettingsService::MakeInputSettings()
    const {
  fuchsia::settings::InputSettings settings;
  std::vector<fuchsia::settings::InputDevice> devices;
  fuchsia::settings::InputDevice mic;
  mic.set_device_type(fuchsia::settings::DeviceType::MICROPHONE);
  fuchsia::settings::DeviceState state;
  state.set_toggle_flags(mic_muted_
                             ? fuchsia::settings::ToggleStateFlags::MUTED
                             : fuchsia::settings::ToggleStateFlags::AVAILABLE);
  mic.set_state(std::move(state));
  devices.push_back(std::move(mic));
  settings.set_devices(std::move(devices));
  return settings;
}

}  // namespace test
