// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/test/cast_runner_launcher.h"

#include <chromium/cast/cpp/fidl.h>
#include <fuchsia/buildinfo/cpp/fidl.h>
#include <fuchsia/camera3/cpp/fidl.h>
#include <fuchsia/component/decl/cpp/fidl.h>
#include <fuchsia/fonts/cpp/fidl.h>
#include <fuchsia/intl/cpp/fidl.h>
#include <fuchsia/kernel/cpp/fidl.h>
#include <fuchsia/legacymetrics/cpp/fidl.h>
#include <fuchsia/logger/cpp/fidl.h>
#include <fuchsia/media/cpp/fidl.h>
#include <fuchsia/memorypressure/cpp/fidl.h>
#include <fuchsia/net/interfaces/cpp/fidl.h>
#include <fuchsia/settings/cpp/fidl.h>
#include <fuchsia/sysmem/cpp/fidl.h>
#include <fuchsia/tracing/provider/cpp/fidl.h>
#include <fuchsia/ui/composition/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/remote_dir.h>

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/run_loop.h"
#include "fuchsia_web/common/test/test_realm_support.h"
#include "media/fuchsia/audio/fake_audio_device_enumerator_local_component.h"

using ::component_testing::ChildRef;
using ::component_testing::Directory;
using ::component_testing::DirectoryContents;
using ::component_testing::FrameworkRef;
using ::component_testing::ParentRef;
using ::component_testing::Protocol;
using ::component_testing::RealmBuilder;
using ::component_testing::Route;
using ::component_testing::Storage;

namespace test {

namespace {

// Name and path under which components must define a writable directory
// capability, for DynamicComponentHost to dynamically offer per-component
// service directories from.
constexpr char kDynamicComponentCapabilitiesName[] =
    "for_dynamic_component_host";
constexpr char kDynamicComponentCapabilitiesPath[] =
    "/for_dynamic_component_host";

class TestProxyLocalComponent : public component_testing::LocalComponentImpl {
 public:
  TestProxyLocalComponent() = default;

  // LocalComponentImpl implementation.
  void OnStart() override {
    // Create a new Directory capability serving the contents of the
    // "for_dynamic_component_host" sub-directory of the calling process'
    // outgoing directory.
    vfs::PseudoDir* capability_dir =
        base::ComponentContextForProcess()->outgoing()->GetOrCreateDirectory(
            kDynamicComponentCapabilitiesName);
    fidl::InterfaceHandle<fuchsia::io::Directory> services;
    zx_status_t status =
        capability_dir->Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                                  fuchsia::io::OpenFlags::RIGHT_WRITABLE |
                                  fuchsia::io::OpenFlags::DIRECTORY,
                              services.NewRequest().TakeChannel());
    ZX_CHECK(status == ZX_OK, status) << "Serve()";

    // Bind that Directory capability under the same path of this virtual
    // component's outgoing-directory, so that attempts to open files under
    // it will be routed back to the calling process' outgoing directory.
    status = outgoing()->root_dir()->AddEntry(
        kDynamicComponentCapabilitiesName,
        std::make_unique<vfs::RemoteDir>(std::move(services)));
    ZX_CHECK(status == ZX_OK, status) << "AddEntry";
  }
};

}  // namespace

CastRunnerLauncher::CastRunnerLauncher(CastRunnerFeatures runner_features) {
  auto realm_builder = RealmBuilder::Create();

  static constexpr char kCastRunnerComponentName[] = "cast_runner";
  realm_builder.AddChild(kCastRunnerComponentName, "#meta/cast_runner.cm");

  base::CommandLine command_line = CommandLineFromFeatures(runner_features);
  static constexpr char const* kSwitchesToCopy[] = {"ozone-platform"};
  command_line.CopySwitchesFrom(*base::CommandLine::ForCurrentProcess(),
                                kSwitchesToCopy);
  AppendCommandLineArguments(realm_builder, kCastRunnerComponentName,
                             command_line);

  // Register the fake fuchsia.feedback service component; plumbing its
  // protocols to cast_runner.
  FakeFeedbackService::RouteToChild(realm_builder, kCastRunnerComponentName);

  AddSyslogRoutesFromParent(realm_builder, kCastRunnerComponentName);

  // Run an isolated font service and route it to cast_runner.
  AddFontService(realm_builder, kCastRunnerComponentName);

  // Run the test-ui-stack and route it to cast_runner.
  AddTestUiStack(realm_builder, kCastRunnerComponentName);

  realm_builder.AddRoute(Route{
      .capabilities =
          {
              // The chromium test realm offers the system-wide config-data dir
              // to test components. Route the cast_runner sub-directory of this
              // to the launched cast_runner component.
              Directory{.name = "config-data", .subdir = "cast_runner"},
              // And route the web_engine sub-directory as required by
              // WebInstanceHost.
              Directory{.name = "config-data",
                        .as = "config-data-for-web-instance",
                        .subdir = "web_engine"},
              Directory{.name = "root-ssl-certificates"},
              Directory{.name = "tzdata-icu"},

              // fuchsia.web/Context required and recommended protocols.
              Protocol{fuchsia::buildinfo::Provider::Name_},
              Protocol{"fuchsia.device.NameProvider"},
              // "fuchsia.fonts.Provider" is provided above.
              Protocol{"fuchsia.hwinfo.Product"},
              Protocol{fuchsia::intl::PropertyProvider::Name_},
              Protocol{fuchsia::kernel::VmexResource::Name_},
              Protocol{"fuchsia.logger.LogSink"},
              Protocol{fuchsia::media::ProfileProvider::Name_},
              Protocol{"fuchsia.scheduler.RoleManager"},
              Protocol{fuchsia::memorypressure::Provider::Name_},
              Protocol{"fuchsia.process.Launcher"},
              Protocol{"fuchsia.sysmem.Allocator"},
              Protocol{"fuchsia.sysmem2.Allocator"},

              // CastRunner sets ContextFeatureFlags::NETWORK by default.
              Protocol{fuchsia::net::interfaces::State::Name_},
              Protocol{"fuchsia.net.name.Lookup"},
              Protocol{"fuchsia.posix.socket.Provider"},

              Storage{.name = "cache", .path = "/cache"},
          },
      .source = ParentRef(),
      .targets = {ChildRef{kCastRunnerComponentName}}});

  // Provide a fake Cast "agent", providing some necessary services.
  static constexpr char kFakeCastAgentName[] = "fake-cast-agent";
  auto fake_cast_agent = std::make_unique<FakeCastAgent>();
  fake_cast_agent_ = fake_cast_agent.get();
  realm_builder.AddLocalChild(
      kFakeCastAgentName,
      [fake_cast_agent = std::move(fake_cast_agent)]() mutable {
        return std::move(fake_cast_agent);
      });
  realm_builder.AddRoute(
      Route{.capabilities =
                {
                    Protocol{chromium::cast::ApplicationConfigManager::Name_},
                    Protocol{chromium::cast::CorsExemptHeaderProvider::Name_},
                    Protocol{fuchsia::camera3::DeviceWatcher::Name_},
                    Protocol{fuchsia::legacymetrics::MetricsRecorder::Name_},
                    Protocol{fuchsia::media::Audio::Name_},
                },
            .source = ChildRef{kFakeCastAgentName},
            .targets = {ChildRef{kCastRunnerComponentName}}});

  if (!(runner_features & kCastRunnerFeaturesHeadless)) {
    // CastRunner sets ThemeType::DEFAULT when not headless.
    AddRouteFromParent(realm_builder, kCastRunnerComponentName,
                       fuchsia::settings::Display::Name_);
  }

  if (runner_features & kCastRunnerFeaturesVulkan) {
    AddVulkanRoutesFromParent(realm_builder, kCastRunnerComponentName);
  }

  // Either route the fake AudioDeviceEnumerator or the system one.
  if (runner_features & kCastRunnerFeaturesFakeAudioDeviceEnumerator) {
    static constexpr char kAudioDeviceEnumerator[] =
        "fake_audio_device_enumerator";
    realm_builder.AddLocalChild(kAudioDeviceEnumerator, []() {
      return std::make_unique<media::FakeAudioDeviceEnumeratorLocalComponent>();
    });
    realm_builder.AddRoute(
        Route{.capabilities = {Protocol{
                  fuchsia::media::AudioDeviceEnumerator::Name_}},
              .source = ChildRef{kAudioDeviceEnumerator},
              .targets = {ChildRef{kCastRunnerComponentName}}});
  } else {
    AddRouteFromParent(realm_builder, kCastRunnerComponentName,
                       fuchsia::media::AudioDeviceEnumerator::Name_);
  }

  // Always offer tracing to CastRunner to suppress log spam. See its CML file.
  AddRouteFromParent(realm_builder, kCastRunnerComponentName,
                     "fuchsia.tracing.provider.Registry");

  // TODO(crbug.com/42050521) Remove once not needed to avoid log spam.
  AddRouteFromParent(realm_builder, kCastRunnerComponentName,
                     "fuchsia.tracing.perfetto.ProducerConnector");

  // Route capabilities from the cast_runner back up to the test.
  realm_builder.AddRoute(
      Route{.capabilities = {Protocol{chromium::cast::DataReset::Name_},
                             Protocol{fuchsia::web::FrameHost::Name_},
                             Protocol{fuchsia::web::Debug::Name_}},
            .source = ChildRef{kCastRunnerComponentName},
            .targets = {ParentRef()}});

  // Define a pseudo-component to bridge from test-scoped outgoing directory
  // into the RealmBuilder scope.
  static constexpr char kTestProxyName[] = "test_proxy";
  realm_builder.AddLocalChild(kTestProxyName,
                              std::make_unique<TestProxyLocalComponent>);

  // Define an environment that uses the exposed Resolver and Runner, and a
  // child component collection that uses that environment, in the test proxy
  // component.
  static constexpr char kCastResolverName[] = "cast-resolver";
  static constexpr char kCastRunnerName[] = "cast-runner";
  {
    auto test_proxy_decl = realm_builder.GetComponentDecl(kTestProxyName);

    static constexpr char kEnvironmentName[] = "cast-test-environment";
    static constexpr char kCastUrlScheme[] = "cast";

    fuchsia::component::decl::ResolverRegistration resolver_decl;
    resolver_decl.set_resolver(kCastResolverName);
    resolver_decl.set_source(fuchsia::component::decl::Ref::WithParent({}));
    resolver_decl.set_scheme(kCastUrlScheme);

    fuchsia::component::decl::RunnerRegistration runner_decl;
    runner_decl.set_source_name(kCastRunnerName);
    runner_decl.set_source(fuchsia::component::decl::Ref::WithParent({}));
    runner_decl.set_target_name(kCastRunnerName);

    fuchsia::component::decl::Environment environment_decl;
    environment_decl.set_name(kEnvironmentName);
    environment_decl.set_extends(
        fuchsia::component::decl::EnvironmentExtends::NONE);
    environment_decl.set_stop_timeout_ms(1000);  // Matches cast_runner.cml
    environment_decl.mutable_resolvers()->emplace_back(
        std::move(resolver_decl));
    environment_decl.mutable_runners()->emplace_back(std::move(runner_decl));
    test_proxy_decl.mutable_environments()->emplace_back(
        std::move(environment_decl));

    fuchsia::component::decl::Collection collection_decl;
    collection_decl.set_name(kTestCollectionName);
    collection_decl.set_environment(kEnvironmentName);
    collection_decl.set_durability(
        fuchsia::component::decl::Durability::TRANSIENT);
    collection_decl.set_allowed_offers(
        fuchsia::component::decl::AllowedOffers::STATIC_AND_DYNAMIC);
    test_proxy_decl.mutable_collections()->emplace_back(
        std::move(collection_decl));

    test_proxy_decl.mutable_capabilities()->emplace_back(
        fuchsia::component::decl::Capability::WithDirectory(std::move(
            fuchsia::component::decl::Directory()
                .set_name(kDynamicComponentCapabilitiesName)
                .set_rights(fuchsia::io::RW_STAR_DIR)
                .set_source_path(kDynamicComponentCapabilitiesPath))));

    test_proxy_decl.mutable_exposes()->emplace_back(
        fuchsia::component::decl::Expose::WithProtocol(std::move(
            fuchsia::component::decl::ExposeProtocol()
                .set_source(fuchsia::component::decl::Ref::WithFramework({}))
                .set_source_name(fuchsia::component::Realm::Name_)
                .set_target(fuchsia::component::decl::Ref::WithParent({}))
                .set_target_name(fuchsia::component::Realm::Name_))));

    realm_builder.ReplaceComponentDecl(kTestProxyName,
                                       std::move(test_proxy_decl));
  }

  // Expose the CastRunner's Realm to the test Realm root, for it to expose
  // for use by integration tests (see below).
  {
    auto runner_decl = realm_builder.GetComponentDecl(kCastRunnerComponentName);
    runner_decl.mutable_exposes()->emplace_back(
        fuchsia::component::decl::Expose::WithProtocol(std::move(
            fuchsia::component::decl::ExposeProtocol()
                .set_source(fuchsia::component::decl::Ref::WithFramework({}))
                .set_source_name(fuchsia::component::Realm::Name_)
                .set_target(fuchsia::component::decl::Ref::WithParent({}))
                .set_target_name(fuchsia::component::Realm::Name_))));
    realm_builder.ReplaceComponentDecl(kCastRunnerComponentName,
                                       std::move(runner_decl));
  }

  // Offer the test-proxy the Cast Resolver and Runner capabilities, and
  // expose its framework-provided Realm protocol out to the test.
  {
    auto realm_decl = realm_builder.GetRealmDecl();
    realm_decl.mutable_exposes()->emplace_back(
        fuchsia::component::decl::Expose::WithProtocol(std::move(
            fuchsia::component::decl::ExposeProtocol()
                .set_source(fuchsia::component::decl::Ref::WithChild(
                    fuchsia::component::decl::ChildRef{.name = kTestProxyName}))
                .set_source_name(fuchsia::component::Realm::Name_)
                .set_target(fuchsia::component::decl::Ref::WithParent({}))
                .set_target_name(fuchsia::component::Realm::Name_))));

    realm_decl.mutable_offers()->emplace_back(
        fuchsia::component::decl::Offer::WithResolver(std::move(
            fuchsia::component::decl::OfferResolver()
                .set_source(fuchsia::component::decl::Ref::WithChild(
                    fuchsia::component::decl::ChildRef{
                        .name = kCastRunnerComponentName}))
                .set_source_name(kCastResolverName)
                .set_target(fuchsia::component::decl::Ref::WithChild(
                    fuchsia::component::decl::ChildRef{.name = kTestProxyName}))
                .set_target_name(kCastResolverName))));

    realm_decl.mutable_offers()->emplace_back(
        fuchsia::component::decl::Offer::WithRunner(std::move(
            fuchsia::component::decl::OfferRunner()
                .set_source(fuchsia::component::decl::Ref::WithChild(
                    fuchsia::component::decl::ChildRef{
                        .name = kCastRunnerComponentName}))
                .set_source_name(kCastRunnerName)
                .set_target(fuchsia::component::decl::Ref::WithChild(
                    fuchsia::component::decl::ChildRef{.name = kTestProxyName}))
                .set_target_name(kCastRunnerName))));

    // Expose the CastRunner's Realm via the root component, as
    // "fuchsia.component.Realm:runner", to allow tests to e.g.
    // manipulate the child components in the `web_instances` collection.
    realm_decl.mutable_exposes()->emplace_back(
        fuchsia::component::decl::Expose::WithProtocol(std::move(
            fuchsia::component::decl::ExposeProtocol()
                .set_source(fuchsia::component::decl::Ref::WithChild(
                    fuchsia::component::decl::ChildRef{
                        .name = kCastRunnerComponentName}))
                .set_source_name(fuchsia::component::Realm::Name_)
                .set_target(fuchsia::component::decl::Ref::WithParent({}))
                .set_target_name(kCastRunnerRealmProtocol))));

    realm_builder.ReplaceRealmDecl(std::move(realm_decl));
  }

  // Create the test realm and connect to the root component's exposed services,
  // for use by tests.
  realm_root_ = realm_builder.Build();
  exposed_services_ = std::make_unique<sys::ServiceDirectory>(
      realm_root_->component().CloneExposedDir());
}

CastRunnerLauncher::~CastRunnerLauncher() {
  if (realm_root_.has_value()) {
    base::RunLoop run_loop;
    realm_root_.value().Teardown(
        [quit = run_loop.QuitClosure()](auto result) { quit.Run(); });
    run_loop.Run();
  }
}

}  // namespace test
