// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/common/test/test_realm_support.h"

#include <lib/sys/component/cpp/testing/realm_builder.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/check.h"
#include "base/command_line.h"

using ::component_testing::ChildRef;
using ::component_testing::Directory;
using ::component_testing::ParentRef;
using ::component_testing::Protocol;
using ::component_testing::RealmBuilder;
using ::component_testing::Route;

namespace test {

void AppendCommandLineArguments(RealmBuilder& realm_builder,
                                base::StringPiece child_name,
                                const base::CommandLine& command_line) {
  const std::string child_name_str(child_name);
  auto context_provider_decl = realm_builder.GetComponentDecl(child_name_str);
  for (auto& entry : *context_provider_decl.mutable_program()
                          ->mutable_info()
                          ->mutable_entries()) {
    if (entry.key == "args") {
      DCHECK(entry.value->is_str_vec());
      entry.value->str_vec().insert(entry.value->str_vec().end(),
                                    command_line.argv().begin() + 1,
                                    command_line.argv().end());
      break;
    }
  }
  realm_builder.ReplaceComponentDecl(child_name_str,
                                     std::move(context_provider_decl));
}

void AddSyslogRoutesFromParent(RealmBuilder& realm_builder,
                               base::StringPiece child_name) {
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{"fuchsia.logger.LogSink"}},
            .source = ParentRef{},
            .targets = {std::move(child_ref)}});
}

void AddVulkanRoutesFromParent(RealmBuilder& realm_builder,
                               base::StringPiece child_name) {
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{"fuchsia.sysmem.Allocator"},
                             Protocol{"fuchsia.tracing.provider.Registry"},
                             Protocol{"fuchsia.vulkan.loader.Loader"}},
            .source = ParentRef{},
            .targets = {std::move(child_ref)}});
}

void AddFontService(RealmBuilder& realm_builder, base::StringPiece child_name) {
  static constexpr char kFontsService[] = "isolated_fonts";
  static constexpr char kFontsUrl[] =
      "fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cm";
  realm_builder.AddChild(kFontsService, kFontsUrl);
  AddSyslogRoutesFromParent(realm_builder, kFontsService);
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
  realm_builder
      .AddRoute(Route{
          .capabilities = {Directory{.name = "config-data", .subdir = "fonts"}},
          .source = ParentRef{},
          .targets = {ChildRef{kFontsService}}})
      .AddRoute(Route{.capabilities = {Protocol{"fuchsia.fonts.Provider"}},
                      .source = ChildRef{kFontsService},
                      .targets = {std::move(child_ref)}});
}

void AddTestUiStack(RealmBuilder& realm_builder, base::StringPiece child_name) {
  static constexpr char kTestUiStackService[] = "test_ui_stack";
  static constexpr char kTestUiStackUrl[] =
      "fuchsia-pkg://fuchsia.com/flatland-scene-manager-test-ui-stack#meta/"
      "test-ui-stack.cm";
  realm_builder.AddChild(kTestUiStackService, kTestUiStackUrl);
  AddSyslogRoutesFromParent(realm_builder, kTestUiStackService);
  AddVulkanRoutesFromParent(realm_builder, kTestUiStackService);
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
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
                      .targets = {std::move(child_ref)}});
}

}  // namespace test
