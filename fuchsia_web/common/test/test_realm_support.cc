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
#include "base/ranges/algorithm.h"

using ::component_testing::ChildRef;
using ::component_testing::Directory;
using ::component_testing::ParentRef;
using ::component_testing::Protocol;
using ::component_testing::RealmBuilder;
using ::component_testing::Route;

namespace test {

namespace {

void AppendCommandLineArgumentsToProgram(
    fuchsia::component::decl::Program& program,
    const base::CommandLine& command_line) {
  // Find the "args" list in the program declaration.
  fuchsia::data::DictionaryEntry* args_entry = nullptr;
  auto* entries = program.mutable_info()->mutable_entries();
  for (auto& entry : *entries) {
    if (entry.key == "args") {
      DCHECK(entry.value->is_str_vec());
      args_entry = &entry;
      break;
    }
  }
  if (!args_entry) {
    // Create a new "args" list and insert it at the proper location in the
    // program's entries; entries' keys must be sorted as per
    // https://fuchsia.dev/reference/fidl/fuchsia.data?hl=en#Dictionary.
    auto lower_bound = base::ranges::lower_bound(
        *entries, "args", /*comp=*/{},
        [](const fuchsia::data::DictionaryEntry& entry) { return entry.key; });
    auto it = entries->emplace(lower_bound);
    it->key = "args";
    it->value = fuchsia::data::DictionaryValue::New();
    it->value->set_str_vec({});
    args_entry = &*it;
  }

  // Append all args following the program name in `command_line` to the
  // program's args.
  args_entry->value->str_vec().insert(args_entry->value->str_vec().end(),
                                      command_line.argv().begin() + 1,
                                      command_line.argv().end());
}

}  // namespace

void AppendCommandLineArguments(RealmBuilder& realm_builder,
                                std::string_view child_name,
                                const base::CommandLine& command_line) {
  const std::string child_name_str(child_name);
  auto child_component_decl = realm_builder.GetComponentDecl(child_name_str);
  AppendCommandLineArgumentsToProgram(*child_component_decl.mutable_program(),
                                      command_line);
  realm_builder.ReplaceComponentDecl(child_name_str,
                                     std::move(child_component_decl));
}

void AppendCommandLineArgumentsForRealm(
    ::component_testing::RealmBuilder& realm_builder,
    const base::CommandLine& command_line) {
  auto decl = realm_builder.GetRealmDecl();
  AppendCommandLineArgumentsToProgram(*decl.mutable_program(), command_line);
  realm_builder.ReplaceRealmDecl(std::move(decl));
}

void AddRouteFromParent(RealmBuilder& realm_builder,
                        std::string_view child_name,
                        std::string_view protocol_name) {
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
  realm_builder.AddRoute(Route{.capabilities = {Protocol{protocol_name}},
                               .source = ParentRef{},
                               .targets = {std::move(child_ref)}});
}

void AddSyslogRoutesFromParent(RealmBuilder& realm_builder,
                               std::string_view child_name) {
  AddRouteFromParent(realm_builder, child_name, "fuchsia.logger.LogSink");
}

void AddVulkanRoutesFromParent(RealmBuilder& realm_builder,
                               std::string_view child_name) {
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{"fuchsia.sysmem.Allocator"},
                             Protocol{"fuchsia.sysmem2.Allocator"},
                             Protocol{"fuchsia.tracing.provider.Registry"},
                             Protocol{"fuchsia.vulkan.loader.Loader"}},
            .source = ParentRef{},
            .targets = {std::move(child_ref)}});
}

void AddFontService(RealmBuilder& realm_builder, std::string_view child_name) {
  static constexpr char kFontsService[] = "isolated_fonts";
  static constexpr char kFontsUrl[] =
      "fuchsia-pkg://fuchsia.com/fonts_hermetic_for_test"
      "#meta/font_provider_hermetic_for_test.cm";
  realm_builder.AddChild(kFontsService, kFontsUrl);
  AddSyslogRoutesFromParent(realm_builder, kFontsService);
  ChildRef child_ref{std::string_view(child_name.data(), child_name.size())};
  realm_builder
      .AddRoute(Route{.capabilities =
                          {
                              Protocol{"fuchsia.tracing.provider.Registry"},
                          },
                      .source = ParentRef{},
                      .targets = {ChildRef{kFontsService}}})
      .AddRoute(Route{.capabilities = {Protocol{"fuchsia.fonts.Provider"}},
                      .source = ChildRef{kFontsService},
                      .targets = {std::move(child_ref)}});
}

void AddTestUiStack(RealmBuilder& realm_builder, std::string_view child_name) {
  static constexpr char kTestUiStackService[] = "test_ui_stack";
  static constexpr char kTestUiStackUrl[] =
      "fuchsia-pkg://fuchsia.com/flatland-scene-manager-test-ui-stack#meta/"
      "test-ui-stack.cm";
  realm_builder.AddChild(kTestUiStackService, kTestUiStackUrl);
  AddRouteFromParent(realm_builder, kTestUiStackService,
                     "fuchsia.scheduler.ProfileProvider");
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
                          },
                      .source = ChildRef{kTestUiStackService},
                      .targets = {std::move(child_ref)}});
}

}  // namespace test
