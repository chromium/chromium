// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/test/context_provider_for_test.h"

#include <lib/sys/cpp/service_directory.h>

#include <utility>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/run_loop.h"
#include "fuchsia_web/common/test/fake_feedback_service.h"
#include "fuchsia_web/common/test/test_realm_support.h"

namespace {

::component_testing::RealmRoot BuildRealm(base::CommandLine command_line) {
  DCHECK(command_line.argv()[0].empty()) << "Must use NO_PROGRAM.";

  auto realm_builder = ::component_testing::RealmBuilder::Create();

  static constexpr char kContextProviderService[] = "context_provider";
  realm_builder.AddChild(kContextProviderService, "#meta/context_provider.cm");

  constexpr char const* kSwitchesToCopy[] = {"ozone-platform"};
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy, std::size(kSwitchesToCopy));

  test::AppendCommandLineArguments(realm_builder, kContextProviderService,
                                   command_line);

  test::FakeFeedbackService::RouteToChild(realm_builder,
                                          kContextProviderService);

  test::AddSyslogRoutesFromParent(realm_builder, kContextProviderService);

  realm_builder
      .AddRoute(::component_testing::Route{
          .capabilities = {::component_testing::Protocol{
                               "fuchsia.sys.Environment"},
                           ::component_testing::Protocol{"fuchsia.sys.Loader"}},
          .source = ::component_testing::ParentRef{},
          .targets = {::component_testing::ChildRef{kContextProviderService}}})
      .AddRoute(::component_testing::Route{
          .capabilities = {::component_testing::Protocol{
                               "fuchsia.web.ContextProvider"},
                           ::component_testing::Protocol{"fuchsia.web.Debug"}},
          .source = ::component_testing::ChildRef{kContextProviderService},
          .targets = {::component_testing::ParentRef{}}});

  return realm_builder.Build();
}

}  // namespace

ContextProviderForTest::ContextProviderForTest(
    const base::CommandLine& command_line)
    : realm_root_(BuildRealm(command_line)) {
  ::fuchsia::web::ContextProviderPtr context_provider;
  zx_status_t status =
      realm_root_.component().Connect(context_provider_.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect to ContextProvider";
}

ContextProviderForTest::~ContextProviderForTest() {
  // We're about to shut down the realm; unbind to unhook the error handler.
  context_provider_.Unbind();
  base::RunLoop run_loop;
  realm_root_.Teardown(
      [quit = run_loop.QuitClosure()](auto result) { quit.Run(); });
  run_loop.Run();
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    const base::CommandLine& command_line)
    : context_provider_(command_line) {}

ContextProviderForDebugTest::~ContextProviderForDebugTest() = default;

void ContextProviderForDebugTest::ConnectToDebug(
    ::fidl::InterfaceRequest<::fuchsia::web::Debug> debug_request) {
  zx_status_t status = context_provider_.realm_root().component().Connect(
      std::move(debug_request));
  ZX_CHECK(status == ZX_OK, status) << "Connect to Debug";
}
