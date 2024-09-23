// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/shell/shell_relauncher.h"

#include <fuchsia/component/cpp/fidl.h>
#include <lib/sys/component/cpp/testing/realm_builder.h>
#include <zircon/types.h>

#include <string>
#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "fuchsia_web/common/test/test_realm_support.h"

std::optional<int> RelaunchForWebInstanceHostIfParent(
    std::string_view relative_component_url,
    const base::CommandLine& command_line) {
  // Nothing to do if running from the context of a relaunched process.
  static constexpr char kNoRelaunch[] = "no-relaunch";
  if (command_line.HasSwitch(kNoRelaunch)) {
    return std::nullopt;
  }

  auto realm_builder = component_testing::RealmBuilder::CreateFromRelativeUrl(
      {relative_component_url.data(), relative_component_url.length()});
  test::AppendCommandLineArgumentsForRealm(realm_builder,  // IN-TEST
                                           command_line);

  auto realm = realm_builder.Build();

  fuchsia::component::BinderPtr binder_proxy =
      realm.component().Connect<fuchsia::component::Binder>();

  // Wait for binder_proxy to be closed.
  base::RunLoop run_loop;
  binder_proxy.set_error_handler(
      [quit_closure = run_loop.QuitClosure()](zx_status_t status) {
        std::move(quit_closure).Run();
      });
  run_loop.Run();

  // Nothing depends on the process exit code of web_engine_shell today, so
  // simply return success in all cases.
  return 0;
}
