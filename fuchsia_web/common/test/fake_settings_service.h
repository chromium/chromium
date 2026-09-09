// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_FAKE_SETTINGS_SERVICE_H_
#define FUCHSIA_WEB_COMMON_TEST_FAKE_SETTINGS_SERVICE_H_

#include <fuchsia/settings/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/component/cpp/testing/realm_builder_types.h>

#include <memory>
#include <string_view>

namespace component_testing {
class RealmBuilder;
}

namespace test {

// Fake implementation of fuchsia.settings.Display and fuchsia.settings.Input
// for use in tests that utilize RealmBuilder.
class FakeSettingsService final
    : public ::component_testing::LocalComponentImpl {
 public:
  FakeSettingsService();
  FakeSettingsService(const FakeSettingsService&) = delete;
  FakeSettingsService& operator=(const FakeSettingsService&) = delete;
  ~FakeSettingsService() override;

  // Registers a LocalComponentFactory function for the FakeSettingsService with
  // RealmBuilder and plumbs its protocols to the peer component identified
  // by the given `child_name`. If `service` is provided, it will be used as the
  // instance; otherwise a default instance is created.
  static void RouteToChild(
      ::component_testing::RealmBuilder& realm_builder,
      std::string_view child_name,
      std::unique_ptr<FakeSettingsService> service = nullptr);

  // Updates the display theme and notifies active Watchers.
  void set_theme(fuchsia::settings::ThemeType theme);

  // Updates the microphone mute state and notifies active Watchers.
  void set_mic_muted(bool muted);

  // ::component_testing::LocalComponentImpl:
  void OnStart() override;

 private:
  // Implements fuchsia.settings.Display for a single client connection.
  class DisplayImpl;

  // Implements fuchsia.settings.Input for a single client connection.
  class InputImpl;

  // Returns DisplaySettings reflecting the current `theme_`.
  fuchsia::settings::DisplaySettings MakeDisplaySettings() const;

  // Returns InputSettings reflecting the current `mic_muted_` state.
  fuchsia::settings::InputSettings MakeInputSettings() const;

  fuchsia::settings::ThemeType theme_ = fuchsia::settings::ThemeType::LIGHT;
  bool mic_muted_ = false;

  fidl::BindingSet<fuchsia::settings::Display, std::unique_ptr<DisplayImpl>>
      display_bindings_;
  fidl::BindingSet<fuchsia::settings::Input, std::unique_ptr<InputImpl>>
      input_bindings_;
};

}  // namespace test

#endif  // FUCHSIA_WEB_COMMON_TEST_FAKE_SETTINGS_SERVICE_H_
