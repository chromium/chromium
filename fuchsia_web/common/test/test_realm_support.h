// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_COMMON_TEST_TEST_REALM_SUPPORT_H_
#define FUCHSIA_WEB_COMMON_TEST_TEST_REALM_SUPPORT_H_

#include <string_view>

namespace base {
class CommandLine;
}

namespace component_testing {
class RealmBuilder;
}

namespace test {

// Appends the arguments of `command_line` (ignoring the program name at
// position zero) to the command line for the component named `child_name`.
void AppendCommandLineArguments(
    ::component_testing::RealmBuilder& realm_builder,
    std::string_view child_name,
    const base::CommandLine& command_line);

// Appends the arguments of `command_line` (ignoring the program name at
// position zero) to the command line for the realm.
void AppendCommandLineArgumentsForRealm(
    ::component_testing::RealmBuilder& realm_builder,
    const base::CommandLine& command_line);

// In the functions below, the term "parent" is the `#realm_builder` collection
// for the test component that is using these helpers to build a test realm.
// Each test component must use a .cml fragment that routes the required
// capabilities to `#realm_builder`.

void AddRouteFromParent(component_testing::RealmBuilder& realm_builder,
                        std::string_view child_name,
                        std::string_view protocol_name);

// Adds routes to the child component named `child_name` to satisfy that
// child's use of syslog/client.shard.cml.
void AddSyslogRoutesFromParent(::component_testing::RealmBuilder& realm_builder,
                               std::string_view child_name);

// Adds routes to the child component named `child_name` to satisfy that
// child's use of vulkan/client.shard.cml.
void AddVulkanRoutesFromParent(::component_testing::RealmBuilder& realm_builder,
                               std::string_view child_name);

// Adds fuchsia-pkg://fuchsia.com/fonts#meta/fonts.cm as a child in the realm,
// routes all of its required capabilities from parent, and routes its
// fuchsia.fonts.Provider protocol to the child component named `child_name`
// in the realm.
void AddFontService(::component_testing::RealmBuilder& realm_builder,
                    std::string_view child_name);

// Adds fuchsia-pkg://fuchsia.com/test-ui-stack#meta/test-ui-stack.cm as a
// child in the realm, routes all of its required capabilities from parent,
// and routes various of its protocols to the child component named
// `child_name` in the realm.
void AddTestUiStack(::component_testing::RealmBuilder& realm_builder,
                    std::string_view child_name);

}  // namespace test

#endif  // FUCHSIA_WEB_COMMON_TEST_TEST_REALM_SUPPORT_H_
