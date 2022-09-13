// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_realm_support.h"

#include <lib/sys/component/cpp/testing/realm_builder.h>

using ::component_testing::ChildRef;
using ::component_testing::Directory;
using ::component_testing::ParentRef;
using ::component_testing::Protocol;
using ::component_testing::RealmBuilder;
using ::component_testing::Route;

namespace test {

void AddSyslogRoutesFromParent(RealmBuilder& realm_builder,
                               std::string_view child_name) {
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{"fuchsia.logger.LogSink"}},
            .source = ParentRef{},
            .targets = {ChildRef{child_name}}});
}

void AddVulkanRoutesFromParent(RealmBuilder& realm_builder,
                               std::string_view child_name) {
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{"fuchsia.sysmem.Allocator"},
                             Protocol{"fuchsia.tracing.provider.Registry"},
                             Protocol{"fuchsia.vulkan.loader.Loader"}},
            .source = ParentRef{},
            .targets = {ChildRef{child_name}}});
}

void AddFontService(RealmBuilder& realm_builder, std::string_view child_name) {
  static constexpr char kFontsService[] = "isolated_fonts";
  static constexpr char kFontsUrl[] =
      "fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cm";
  realm_builder.AddChild(kFontsService, kFontsUrl);
  AddSyslogRoutesFromParent(realm_builder, kFontsService);
  realm_builder
      .AddRoute(Route{
          .capabilities = {Directory{.name = "config-data", .subdir = "fonts"}},
          .source = ParentRef{},
          .targets = {ChildRef{kFontsService}}})
      .AddRoute(Route{.capabilities = {Protocol{"fuchsia.fonts.Provider"}},
                      .source = ChildRef{kFontsService},
                      .targets = {ChildRef{child_name}}});
}

void AddTestUiStack(RealmBuilder& realm_builder, std::string_view child_name) {
  static constexpr char kTestUiStackService[] = "test_ui_stack";
  static constexpr char kTestUiStackUrl[] =
      "fuchsia-pkg://fuchsia.com/flatland-scene-manager-test-ui-stack#meta/"
      "test-ui-stack.cm";
  realm_builder.AddChild(kTestUiStackService, kTestUiStackUrl);
  AddSyslogRoutesFromParent(realm_builder, kTestUiStackService);
  AddVulkanRoutesFromParent(realm_builder, kTestUiStackService);
  realm_builder
      .AddRoute(
          Route{.capabilities = {Protocol{"fuchsia.scheduler.ProfileProvider"}},
                .source = ParentRef(),
                .targets = {ChildRef{kTestUiStackService}}})
      .AddRoute(Route{.capabilities =
                          {
                              Protocol{"fuchsia.ui.composition.Allocator"},
                              Protocol{"fuchsia.ui.composition.Flatland"},
                              Protocol{"fuchsia.ui.scenic.Scenic"},
                          },
                      .source = ChildRef{kTestUiStackService},
                      .targets = {ChildRef{child_name}}});
}

}  // namespace test
