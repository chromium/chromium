// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/context_provider_impl.h"

#include <fuchsia/sys/cpp/fidl_test_base.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/zx/socket.h>
#include <zircon/processargs.h>
#include <zircon/types.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "fuchsia_web/webengine/fake_context.h"
#include "fuchsia_web/webengine/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "third_party/widevine/cdm/buildflags.h"

namespace {

constexpr char kTestDataFileIn[] = "DataFileIn";
constexpr char kTestDataFileOut[] = "DataFileOut";

constexpr char kUrl[] = "chrome://:emorhc";
constexpr char kTitle[] = "Palindrome";

constexpr uint64_t kTestQuotaBytes = 1024;
constexpr char kTestQuotaBytesSwitchValue[] = "1024";

MULTIPROCESS_TEST_MAIN(SpawnContextServer) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);

  LOG(INFO) << "SpawnContextServer test component started.";

  base::FilePath data_dir(base::kPersistedDataDirectoryPath);
  if (base::PathExists(data_dir.AppendASCII(kTestDataFileIn))) {
    auto out_file = data_dir.AppendASCII(kTestDataFileOut);
    EXPECT_EQ(base::WriteFile(out_file, nullptr, 0), 0);
  }

  // Publish the fake fuchsia.web.Context implementation for the test to use.
  FakeContext context;
  fidl::BindingSet<fuchsia::web::Context> bindings;
  base::ComponentContextForProcess()->outgoing()->AddPublicService(
      bindings.GetHandler(&context), "fuchsia.web.Context");
  base::ComponentContextForProcess()->outgoing()->ServeFromStartupInfo();

  // When a Frame's NavigationEventListener is bound, immediately broadcast a
  // navigation event to its listeners.
  context.set_on_create_frame_callback(
      base::BindRepeating([](FakeFrame* frame) {
        frame->set_on_set_listener_callback(base::BindOnce(
            [](FakeFrame* frame) {
              fuchsia::web::NavigationState state;
              state.set_url(kUrl);
              state.set_title(kTitle);
              frame->listener()->OnNavigationStateChanged(std::move(state),
                                                          []() {});
            },
            frame));
      }));

  // Quit the process when the context is destroyed.
  base::RunLoop run_loop;
  bindings.set_empty_set_handler(
      [quit_loop = run_loop.QuitClosure()]() { quit_loop.Run(); });
  run_loop.Run();

  return 0;
}

// Fake implementation of the Launcher for the isolated environment in which
// web instance Components are launched.
class FakeSysLauncher final : public fuchsia::sys::testing::Launcher_TestBase {
 public:
  using CreateComponentCallback =
      base::OnceCallback<void(const base::CommandLine&)>;

  explicit FakeSysLauncher(fuchsia::sys::Launcher* real_launcher)
      : real_launcher_(real_launcher) {}
  ~FakeSysLauncher() override = default;

  void set_create_component_callback(CreateComponentCallback callback) {
    create_component_callback_ = std::move(callback);
  }

  void Bind(fidl::InterfaceRequest<fuchsia::sys::Launcher> request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // fuchsia::sys::Launcher implementation.
  void CreateComponent(fuchsia::sys::LaunchInfo launch_info,
                       fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                           request) override {
    // |arguments| should not include argv[0] (i.e. the program name), which
    // would be empty in a no-program CommandLine instance. Verify that the
    // |arguments| are either empty or have a non-empty first element.
    EXPECT_TRUE(launch_info.arguments->empty() ||
                !launch_info.arguments->at(0).empty());

    // |arguments| omits argv[0] so cannot be used directly to initialize a
    // CommandLine, but CommandLine provides useful switch processing logic.
    // Prepend an empty element to a copy of |arguments| and use that to create
    // a valid CommandLine.
    std::vector<std::string> command_line_args(*launch_info.arguments);
    command_line_args.emplace(command_line_args.begin());
    const base::CommandLine command_line(command_line_args);
    CHECK(!command_line.HasSwitch(switches::kTestChildProcess));

    // If a create-component-callback is specified then there is no need to
    // actually launch a component.
    if (create_component_callback_) {
      std::move(create_component_callback_).Run(command_line);
      return;
    }

    // Otherwise, launch another instance of this test executable, configured to
    // run as a test child (similar to SpawnMultiProcessTestChild()). The
    // test-suite's component manifest cannot be re-used for this because it
    // specifies the "isolated-persistent-data" feature, causing the framework
    // to populate /data, which prevents the |data_directory| supplied in the
    // CreateContextParams from being mapped.
    // Launch the component via a fake manifest identical to the one used for
    // web instances, but which runs this test executable.
    EXPECT_EQ(launch_info.url,
              "fuchsia-pkg://fuchsia.com/web_engine#meta/web_instance.cmx");
    launch_info.url =
        "fuchsia-pkg://fuchsia.com/web_engine_unittests#meta/"
        "web_engine_unittests_fake_instance.cmx";
    launch_info.arguments->push_back(base::StrCat(
        {"--", switches::kTestChildProcess, "=SpawnContextServer"}));

    // Bind /tmp in the new Component's flat namespace, to allow it to see
    // the GTest flagfile, if any.
    fuchsia::io::DirectoryHandle tmp_directory;
    zx_status_t status = fdio_open(
        "/tmp", static_cast<uint32_t>(fuchsia::io::OpenFlags::RIGHT_READABLE),
        tmp_directory.NewRequest().TakeChannel().release());
    ZX_CHECK(status == ZX_OK, status) << "fdio_open(/tmp)";
    launch_info.flat_namespace->paths.push_back("/tmp");
    launch_info.flat_namespace->directories.push_back(std::move(tmp_directory));

    // Redirect the sub-process Component's stderr to feed into the test output.
    launch_info.err = fuchsia::sys::FileDescriptor::New();
    launch_info.err->type0 = PA_FD;
    status = fdio_fd_clone(STDERR_FILENO,
                           launch_info.err->handle0.reset_and_get_address());
    ZX_CHECK(status == ZX_OK, status);

    real_launcher_->CreateComponent(std::move(launch_info), std::move(request));
  }

 private:
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unexpected call: " << name;
  }

  fidl::BindingSet<fuchsia::sys::Launcher> bindings_;
  fuchsia::sys::Launcher* const real_launcher_;
  CreateComponentCallback create_component_callback_;
};

// Fake implementation of the isolated Environment created by ContextProvider.
class FakeNestedSysEnvironment
    : public fuchsia::sys::testing::Environment_TestBase {
 public:
  explicit FakeNestedSysEnvironment(FakeSysLauncher* fake_launcher)
      : fake_launcher_(fake_launcher) {}
  ~FakeNestedSysEnvironment() override = default;

  void Bind(fidl::InterfaceRequest<fuchsia::sys::Environment> request) {
    bindings_.AddBinding(this, std::move(request));
  }

  // fuchsia::sys::Environment implementation.
  void GetLauncher(fidl::InterfaceRequest<fuchsia::sys::Launcher>
                       launcher_request) override {
    fake_launcher_->Bind(std::move(launcher_request));
  }

 private:
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unexpected call: " << name;
  }

  FakeSysLauncher* const fake_launcher_;
  fidl::BindingSet<fuchsia::sys::Environment> bindings_;
};

// Fake implementation of the Environment in which the ContextProvider runs.
class FakeSysEnvironment final
    : public fuchsia::sys::testing::Environment_TestBase {
 public:
  FakeSysEnvironment(sys::OutgoingDirectory* outgoing_directory,
                     fuchsia::sys::Launcher* real_launcher)
      : bindings_(outgoing_directory, this),
        fake_launcher_(real_launcher),
        fake_nested_environment_(&fake_launcher_) {}
  ~FakeSysEnvironment() override = default;

  FakeSysLauncher& fake_launcher() { return fake_launcher_; }

  // fuchsia::sys::Environment implementation.
  void CreateNestedEnvironment(
      fidl::InterfaceRequest<fuchsia::sys::Environment> environment_request,
      fidl::InterfaceRequest<fuchsia::sys::EnvironmentController>
          controller_request,
      std::string label,
      fuchsia::sys::ServiceListPtr additional_services,
      fuchsia::sys::EnvironmentOptions options) override {
    EXPECT_TRUE(environment_request);
    EXPECT_TRUE(controller_request);
    EXPECT_FALSE(label.empty());

    // The nested environment should receive only the Loader service.
    ASSERT_TRUE(additional_services);
    ASSERT_EQ(additional_services->names.size(), 1u);
    EXPECT_EQ(additional_services->names[0], "fuchsia.sys.Loader");
    EXPECT_TRUE(additional_services->host_directory);

    EXPECT_FALSE(options.inherit_parent_services);
    EXPECT_FALSE(options.use_parent_runners);
    EXPECT_TRUE(options.delete_storage_on_death);

    fake_nested_environment_.Bind(std::move(environment_request));
    nested_environment_controller_request_ = std::move(controller_request);
  }
  void GetDirectory(
      fidl::InterfaceRequest<::fuchsia::io::Directory> request) override {
    base::ComponentContextForProcess()->svc()->CloneChannel(std::move(request));
  }

 private:
  void NotImplemented_(const std::string& name) override {
    ADD_FAILURE() << "Unexpected call: " << name;
  }

  base::ScopedServiceBinding<fuchsia::sys::Environment> bindings_;
  FakeSysLauncher fake_launcher_;
  FakeNestedSysEnvironment fake_nested_environment_;

  fidl::InterfaceRequest<fuchsia::sys::EnvironmentController>
      nested_environment_controller_request_;
};

fuchsia::web::CreateContextParams BuildCreateContextParams() {
  fuchsia::web::CreateContextParams output;
  zx_status_t result = fdio_service_connect(
      base::kServiceDirectoryPath,
      output.mutable_service_directory()->NewRequest().TakeChannel().release());
  ZX_CHECK(result == ZX_OK, result) << "Failed to open /svc";
  return output;
}

fidl::InterfaceHandle<fuchsia::io::Directory> OpenCacheDirectory() {
  fidl::InterfaceHandle<fuchsia::io::Directory> cache_handle;
  zx_status_t result =
      fdio_service_connect(base::kPersistedCacheDirectoryPath,
                           cache_handle.NewRequest().TakeChannel().release());
  ZX_CHECK(result == ZX_OK, result) << "Failed to open /cache";
  return cache_handle;
}

}  // namespace

class ContextProviderImplTest : public base::MultiProcessTest {
 public:
  ContextProviderImplTest()
      : sys_launcher_(base::ComponentContextForProcess()
                          ->svc()
                          ->Connect<fuchsia::sys::Launcher>()),
        fake_environment_(test_component_context_.additional_services(),
                          sys_launcher_.get()),
        provider_(std::make_unique<ContextProviderImpl>()) {
    bindings_.AddBinding(provider_.get(), provider_ptr_.NewRequest());
  }

  ContextProviderImplTest(const ContextProviderImplTest&) = delete;
  ContextProviderImplTest& operator=(const ContextProviderImplTest&) = delete;

  ~ContextProviderImplTest() override {
    provider_ptr_.Unbind();
    base::RunLoop().RunUntilIdle();
  }

  // Check if a Context is responsive by creating a Frame from it and then
  // listening for an event.
  void CheckContextResponsive(
      fidl::InterfacePtr<fuchsia::web::Context>* context) {
    // Call a Context method and wait for it to invoke a listener call.
    base::RunLoop run_loop;
    context->set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
          quit_loop.Run();
          ZX_LOG(ERROR, status) << " Context lost.";
          ADD_FAILURE();
        });

    fuchsia::web::FramePtr frame_ptr;
    frame_ptr.set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
          quit_loop.Run();
          ZX_LOG(ERROR, status) << " Frame lost.";
          ADD_FAILURE();
        });
    (*context)->CreateFrame(frame_ptr.NewRequest());

    // Create a Frame and expect to see a navigation event.
    CapturingNavigationStateObserver change_listener(run_loop.QuitClosure());
    fidl::Binding<fuchsia::web::NavigationEventListener>
        change_listener_binding(&change_listener);
    frame_ptr->SetNavigationEventListener2(change_listener_binding.NewBinding(),
                                           /*flags=*/{});
    run_loop.Run();

    ASSERT_TRUE(change_listener.captured_state()->has_url());
    EXPECT_EQ(change_listener.captured_state()->url(), kUrl);
    ASSERT_TRUE(change_listener.captured_state()->has_title());
    EXPECT_EQ(change_listener.captured_state()->title(), kTitle);
  }

  // Checks that the Context channel was dropped.
  void CheckContextUnresponsive(
      fidl::InterfacePtr<fuchsia::web::Context>* context) {
    base::RunLoop run_loop;
    context->set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
          quit_loop.Run();
          EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
        });

    fuchsia::web::FramePtr frame;
    (*context)->CreateFrame(frame.NewRequest());

    // The error handler should be called here.
    run_loop.Run();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // fuchsia.sys.Launcher member must be constructed before the test component
  // context replaces the process' component context.
  fuchsia::sys::LauncherPtr sys_launcher_;

  // Used to replace the process component context with one providing a fake
  // fuchsia.sys.Environment, through which a nested Environment and fake
  // Launcher are obtained.
  base::TestComponentContextForProcess test_component_context_;
  FakeSysEnvironment fake_environment_;

  std::unique_ptr<ContextProviderImpl> provider_;
  fuchsia::web::ContextProviderPtr provider_ptr_;
  fidl::BindingSet<fuchsia::web::ContextProvider> bindings_;

 private:
  struct CapturingNavigationStateObserver
      : public fuchsia::web::NavigationEventListener {
   public:
    explicit CapturingNavigationStateObserver(base::OnceClosure on_change_cb)
        : on_change_cb_(std::move(on_change_cb)) {}
    ~CapturingNavigationStateObserver() override = default;

    void OnNavigationStateChanged(
        fuchsia::web::NavigationState change,
        OnNavigationStateChangedCallback callback) override {
      captured_state_ = std::move(change);
      std::move(on_change_cb_).Run();
    }

    fuchsia::web::NavigationState* captured_state() { return &captured_state_; }

   private:
    base::OnceClosure on_change_cb_;
    fuchsia::web::NavigationState captured_state_;
  };
};

TEST_F(ContextProviderImplTest, CanCreateContext) {
  // Connect to a new context process.
  fidl::InterfacePtr<fuchsia::web::Context> context;
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  provider_ptr_->Create(std::move(create_params), context.NewRequest());
  CheckContextResponsive(&context);
}

TEST_F(ContextProviderImplTest, CreateValidatesServiceDirectory) {
  // Attempt to create a Context without specifying a service directory.
  fidl::InterfacePtr<fuchsia::web::Context> context;
  fuchsia::web::CreateContextParams create_params;
  provider_ptr_->Create(std::move(create_params), context.NewRequest());
  base::RunLoop run_loop;
  context.set_error_handler(
      [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
        quit_loop.Run();
        EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
      });
  run_loop.Run();
}

TEST_F(ContextProviderImplTest, CreateValidatesDataDirectory) {
  // Deliberately supply the wrong kind of object as the data-directory.
  fidl::InterfacePtr<fuchsia::web::Context> context;
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  zx::socket socket1, socket2;
  ASSERT_EQ(zx::socket::create(0, &socket1, &socket2), ZX_OK);
  create_params.set_data_directory(
      fidl::InterfaceHandle<fuchsia::io::Directory>(
          zx::channel(socket1.release())));
  provider_ptr_->Create(std::move(create_params), context.NewRequest());
  base::RunLoop run_loop;
  context.set_error_handler(
      [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
        quit_loop.Run();
        EXPECT_TRUE(status == ZX_ERR_PEER_CLOSED);
      });
  run_loop.Run();
}

TEST_F(ContextProviderImplTest, CreateValidatesDrmFlags) {
  {
    // Request Widevine DRM but do not enable VULKAN.
    fidl::InterfacePtr<fuchsia::web::Context> context;
    fuchsia::web::CreateContextParams create_params =
        BuildCreateContextParams();
    *create_params.mutable_features() =
        fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
    *create_params.mutable_cdm_data_directory() = OpenCacheDirectory();
    provider_ptr_->Create(std::move(create_params), context.NewRequest());
    base::RunLoop run_loop;
    context.set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
          quit_loop.Run();
          EXPECT_EQ(status, ZX_ERR_NOT_SUPPORTED);
        });
    run_loop.Run();
  }

  {
    // Request PlayReady DRM but do not enable VULKAN.
    fidl::InterfacePtr<fuchsia::web::Context> context;
    fuchsia::web::CreateContextParams create_params =
        BuildCreateContextParams();
    create_params.set_playready_key_system("foo");
    *create_params.mutable_cdm_data_directory() = OpenCacheDirectory();
    provider_ptr_->Create(std::move(create_params), context.NewRequest());
    base::RunLoop run_loop;
    context.set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
          quit_loop.Run();
          EXPECT_EQ(status, ZX_ERR_NOT_SUPPORTED);
        });
    run_loop.Run();
  }

  {
    // Requesting DRM without VULKAN is acceptable for HEADLESS Contexts.
    fidl::InterfacePtr<fuchsia::web::Context> context;
    fuchsia::web::CreateContextParams create_params =
        BuildCreateContextParams();
    *create_params.mutable_features() =
        fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM |
        fuchsia::web::ContextFeatureFlags::HEADLESS;
    *create_params.mutable_cdm_data_directory() = OpenCacheDirectory();
    provider_ptr_->Create(std::move(create_params), context.NewRequest());
    base::RunLoop run_loop;
    context.set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t status) {
          quit_loop.Run();
          ZX_LOG(ERROR, status);
          ADD_FAILURE();
        });
    // Spin the loop to allow CreateContext() to be handled, and the |context|
    // channel to be disconnected, in case of failure.
    run_loop.RunUntilIdle();
  }
}

TEST_F(ContextProviderImplTest, MultipleConcurrentClients) {
  // Bind a Provider connection, and create a Context from it.
  fuchsia::web::ContextProviderPtr provider_1_ptr;
  bindings_.AddBinding(provider_.get(), provider_1_ptr.NewRequest());
  fuchsia::web::ContextPtr context_1;
  provider_1_ptr->Create(BuildCreateContextParams(), context_1.NewRequest());

  // Do the same on another Provider connection.
  fuchsia::web::ContextProviderPtr provider_2_ptr;
  bindings_.AddBinding(provider_.get(), provider_2_ptr.NewRequest());
  fuchsia::web::ContextPtr context_2;
  provider_2_ptr->Create(BuildCreateContextParams(), context_2.NewRequest());

  CheckContextResponsive(&context_1);
  CheckContextResponsive(&context_2);

  // Ensure that the initial ContextProvider connection is still usable, by
  // creating and verifying another Context from it.
  fuchsia::web::ContextPtr context_3;
  provider_2_ptr->Create(BuildCreateContextParams(), context_3.NewRequest());
  CheckContextResponsive(&context_3);
}

TEST_F(ContextProviderImplTest, WithProfileDir) {
  base::ScopedTempDir profile_temp_dir;

  // Connect to a new context process.
  fidl::InterfacePtr<fuchsia::web::Context> context;
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();

  // Setup data dir.
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::WriteFile(profile_temp_dir.GetPath().AppendASCII(kTestDataFileIn),
                      base::StringPiece()));

  // Pass a handle data dir to the context.
  create_params.set_data_directory(
      base::OpenDirectoryHandle(profile_temp_dir.GetPath()));

  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  CheckContextResponsive(&context);

  // Verify that the context process can write to the data dir.
  EXPECT_TRUE(base::PathExists(
      profile_temp_dir.GetPath().AppendASCII(kTestDataFileOut)));
}

TEST_F(ContextProviderImplTest, FailsDataDirectoryIsFile) {
  base::FilePath temp_file_path;

  // Connect to a new context process.
  fidl::InterfacePtr<fuchsia::web::Context> context;
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();

  // Pass in a handle to a file instead of a directory.
  CHECK(base::CreateTemporaryFile(&temp_file_path));
  create_params.set_data_directory(base::OpenDirectoryHandle(temp_file_path));

  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  CheckContextUnresponsive(&context);
}

// Tests that unsafely_treat_insecure_origins_as_secure properly adds the right
// command-line arguments to the Context process.
TEST_F(ContextProviderImplTest, WithInsecureOriginsAsSecure) {
  base::RunLoop loop;
  fake_environment_.fake_launcher().set_create_component_callback(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command) {
        const char* kAllowRunningInsecureContent =
            "allow-running-insecure-content";
        loop.Quit();
        EXPECT_TRUE(command.HasSwitch(
            network::switches::kUnsafelyTreatInsecureOriginAsSecure));
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
        ASSERT_STREQ(kAllowRunningInsecureContent,
                     switches::kAllowRunningInsecureContent);
        EXPECT_TRUE(command.HasSwitch(kAllowRunningInsecureContent));
        EXPECT_THAT(command.GetSwitchValueASCII(switches::kDisableFeatures),
                    testing::HasSubstr("AutoupgradeMixedContent"));
        EXPECT_EQ(command.GetSwitchValueASCII(
                      network::switches::kUnsafelyTreatInsecureOriginAsSecure),
                  "http://example.com,http://example.net");
#else
        EXPECT_FALSE(command.HasSwitch(kAllowRunningInsecureContent));
        EXPECT_FALSE(command.HasSwitch(switches::kDisableFeatures));

        // The unrecognized values are passed on as origins.
        EXPECT_EQ(command.GetSwitchValueASCII(
                      network::switches::kUnsafelyTreatInsecureOriginAsSecure),
                  "allow-running-insecure-content,"
                  "disable-mixed-content-autoupgrade,"
                  "http://example.com,http://example.net");
#endif
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  std::vector<std::string> insecure_origins;
  insecure_origins.push_back("allow-running-insecure-content");
  insecure_origins.push_back("disable-mixed-content-autoupgrade");
  insecure_origins.push_back("http://example.com");
  insecure_origins.push_back("http://example.net");
  create_params.set_unsafely_treat_insecure_origins_as_secure(
      std::move(insecure_origins));
  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  loop.Run();
}

TEST_F(ContextProviderImplTest, WithDataQuotaBytes) {
  base::RunLoop loop;
  fake_environment_.fake_launcher().set_create_component_callback(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command) {
        loop.Quit();
        EXPECT_EQ(command.GetSwitchValueASCII("data-quota-bytes"),
                  kTestQuotaBytesSwitchValue);
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  base::ScopedTempDir profile_temp_dir;
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());
  create_params.set_data_directory(
      base::OpenDirectoryHandle(profile_temp_dir.GetPath()));
  create_params.set_data_quota_bytes(kTestQuotaBytes);
  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  loop.Run();
}

TEST_F(ContextProviderImplTest, WithCdmDataQuotaBytes) {
  base::RunLoop loop;
  fake_environment_.fake_launcher().set_create_component_callback(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command) {
        loop.Quit();
        EXPECT_EQ(command.GetSwitchValueASCII("cdm-data-quota-bytes"),
                  kTestQuotaBytesSwitchValue);
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  base::ScopedTempDir profile_temp_dir;
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());
  create_params.set_cdm_data_directory(
      base::OpenDirectoryHandle(profile_temp_dir.GetPath()));
  create_params.set_features(fuchsia::web::ContextFeatureFlags::HEADLESS |
                             fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM);
  create_params.set_cdm_data_quota_bytes(kTestQuotaBytes);
  provider_ptr_->Create(std::move(create_params), context.NewRequest());

  loop.Run();
}
