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

  static constexpr char const* kSwitchesToCopy[] = {"ozone-platform"};
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy);

  test::AppendCommandLineArguments(realm_builder, kContextProviderService,
                                   command_line);

  test::FakeFeedbackService::RouteToChild(realm_builder,
                                          kContextProviderService);

  test::AddSyslogRoutesFromParent(realm_builder, kContextProviderService);

  realm_builder
      .AddRoute(::component_testing::Route{
          .capabilities =
              {// Capabilities used/routed by WebInstanceHost:
               ::component_testing::Directory{"config-data-for-web-instance"},
               ::component_testing::Directory{"tzdata-icu"},
               // Required capabilities offered to web-instance.cm:
               ::component_testing::Directory{"root-ssl-certificates"},
               ::component_testing::Protocol{"fuchsia.buildinfo.Provider"},
               ::component_testing::Protocol{"fuchsia.device.NameProvider"},
               ::component_testing::Protocol{"fuchsia.fonts.Provider"},
               ::component_testing::Protocol{"fuchsia.hwinfo.Product"},
               ::component_testing::Protocol{"fuchsia.intl.PropertyProvider"},
               ::component_testing::Protocol{"fuchsia.kernel.VmexResource"},
               ::component_testing::Protocol{"fuchsia.logger.LogSink"},
               ::component_testing::Protocol{"fuchsia.memorypressure.Provider"},
               ::component_testing::Protocol{"fuchsia.process.Launcher"},
               ::component_testing::Protocol{"fuchsia.sysmem.Allocator"},
               ::component_testing::Protocol{"fuchsia.sysmem2.Allocator"},
               // Optional capabilities offered to web-instance.cm:
               ::component_testing::Protocol{"fuchsia.camera3.DeviceWatcher"},
               ::component_testing::Protocol{"fuchsia.media.ProfileProvider"},
               ::component_testing::Protocol{"fuchsia.scheduler.RoleManager"},
               ::component_testing::Protocol{"fuchsia.settings.Display"},
               ::component_testing::Protocol{
                   "fuchsia.tracing.perfetto.ProducerConnector"},
               ::component_testing::Protocol{
                   "fuchsia.tracing.provider.Registry"}},
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

// static
ContextProviderForTest ContextProviderForTest::Create(
    const base::CommandLine& command_line) {
  auto realm_root = BuildRealm(command_line);
  ::fuchsia::web::ContextProviderPtr context_provider;
  zx_status_t status =
      realm_root.component().Connect(context_provider.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect to ContextProvider";
  return ContextProviderForTest(std::move(realm_root),
                                std::move(context_provider));
}

ContextProviderForTest::ContextProviderForTest(
    ContextProviderForTest&&) noexcept = default;
ContextProviderForTest& ContextProviderForTest::operator=(
    ContextProviderForTest&&) noexcept = default;

ContextProviderForTest::~ContextProviderForTest() {
  // We're about to shut down the realm; unbind to unhook the error handler.
  context_provider_.Unbind();
  base::RunLoop run_loop;
  realm_root_.Teardown(
      [quit = run_loop.QuitClosure()](auto result) { quit.Run(); });
  run_loop.Run();
}

ContextProviderForTest::ContextProviderForTest(
    ::component_testing::RealmRoot realm_root,
    ::fuchsia::web::ContextProviderPtr context_provider)
    : realm_root_(std::move(realm_root)),
      context_provider_(std::move(context_provider)) {}

// static
ContextProviderForDebugTest ContextProviderForDebugTest::Create(
    const base::CommandLine& command_line) {
  return ContextProviderForDebugTest(
      ContextProviderForTest::Create(command_line));
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    ContextProviderForDebugTest&&) noexcept = default;
ContextProviderForDebugTest& ContextProviderForDebugTest::operator=(
    ContextProviderForDebugTest&&) noexcept = default;
ContextProviderForDebugTest::~ContextProviderForDebugTest() = default;

void ContextProviderForDebugTest::ConnectToDebug(
    ::fidl::InterfaceRequest<::fuchsia::web::Debug> debug_request) {
  zx_status_t status = context_provider_.realm_root().component().Connect(
      std::move(debug_request));
  ZX_CHECK(status == ZX_OK, status) << "Connect to Debug";
}

ContextProviderForDebugTest::ContextProviderForDebugTest(
    ContextProviderForTest context_provider)
    : context_provider_(std::move(context_provider)) {}
