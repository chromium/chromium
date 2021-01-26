// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/context_provider_impl.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/zx/socket.h>
#include <zircon/processargs.h>
#include <zircon/types.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include "base/base_paths_fuchsia.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/default_job.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "base/test/multiprocess_test.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "build/build_config.h"
#include "fuchsia/engine/context_provider_impl.h"
#include "fuchsia/engine/fake_context.h"
#include "fuchsia/engine/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

namespace {

constexpr char kTestDataFileIn[] = "DataFileIn";
constexpr char kTestDataFileOut[] = "DataFileOut";

constexpr char kUrl[] = "chrome://:emorhc";
constexpr char kTitle[] = "Palindrome";

constexpr uint64_t kTestQuotaBytes = 1024;
constexpr char kTestQuotaBytesSwitchValue[] = "1024";

constexpr char kCommandLineArgs[] = "command-line-args";

MULTIPROCESS_TEST_MAIN(SpawnContextServer) {
  base::test::SingleThreadTaskEnvironment task_environment(
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO);

  base::FilePath data_dir;
  CHECK(base::PathService::Get(base::DIR_APP_DATA, &data_dir));
  if (!data_dir.empty()) {
    if (base::PathExists(data_dir.AppendASCII(kTestDataFileIn))) {
      auto out_file = data_dir.AppendASCII(kTestDataFileOut);
      EXPECT_EQ(base::WriteFile(out_file, nullptr, 0), 0);
    }
  }

  fidl::InterfaceRequest<fuchsia::web::Context> fuchsia_context(zx::channel(
      zx_take_startup_handle(ContextProviderImpl::kContextRequestHandleId)));
  CHECK(fuchsia_context);

  FakeContext context;
  fidl::Binding<fuchsia::web::Context> context_binding(
      &context, std::move(fuchsia_context));

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
  context_binding.set_error_handler([&run_loop](zx_status_t status) {
    run_loop.Quit();
    EXPECT_EQ(status, ZX_ERR_PEER_CLOSED);
  });
  run_loop.Run();

  return 0;
}

base::Process LaunchFakeContextProcess(const base::CommandLine& command_line,
                                       const base::LaunchOptions& options) {
  base::LaunchOptions options_with_tmp = options;
  options_with_tmp.paths_to_clone.push_back(base::FilePath("/tmp"));
  return base::SpawnMultiProcessTestChild("SpawnContextServer", command_line,
                                          options_with_tmp);
}

fuchsia::web::CreateContextParams BuildCreateContextParams() {
  fuchsia::web::CreateContextParams output;
  zx_status_t result = fdio_service_connect(
      base::kServiceDirectoryPath,
      output.mutable_service_directory()->NewRequest().TakeChannel().release());
  ZX_CHECK(result == ZX_OK, result) << "Failed to open /svc";
  return output;
}

base::Value CreateConfigWithSwitchValue(std::string switch_name,
                                        std::string switch_value) {
  base::Value config_dict(base::Value::Type::DICTIONARY);
  base::Value args(base::Value::Type::DICTIONARY);
  args.SetStringKey(switch_name, switch_value);
  config_dict.SetKey(kCommandLineArgs, std::move(args));
  return config_dict;
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
      : provider_(std::make_unique<ContextProviderImpl>()) {
    provider_->SetLaunchCallbackForTest(
        base::BindRepeating(&LaunchFakeContextProcess));
    bindings_.AddBinding(provider_.get(), provider_ptr_.NewRequest());
  }

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
    context->set_error_handler([&run_loop](zx_status_t status) {
      run_loop.Quit();
      ZX_LOG(ERROR, status) << " Context lost.";
      ADD_FAILURE();
    });

    fuchsia::web::FramePtr frame_ptr;
    frame_ptr.set_error_handler([&run_loop](zx_status_t status) {
      run_loop.Quit();
      ZX_LOG(ERROR, status) << " Frame lost.";
      ADD_FAILURE();
    });
    (*context)->CreateFrame(frame_ptr.NewRequest());

    // Create a Frame and expect to see a navigation event.
    CapturingNavigationStateObserver change_listener(run_loop.QuitClosure());
    fidl::Binding<fuchsia::web::NavigationEventListener>
        change_listener_binding(&change_listener);
    frame_ptr->SetNavigationEventListener(change_listener_binding.NewBinding());
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
    context->set_error_handler([&run_loop](zx_status_t status) {
      run_loop.Quit();
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

  DISALLOW_COPY_AND_ASSIGN(ContextProviderImplTest);
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
  context.set_error_handler([&run_loop](zx_status_t status) {
    run_loop.Quit();
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
  context.set_error_handler([&run_loop](zx_status_t status) {
    run_loop.Quit();
    EXPECT_EQ(status, ZX_ERR_INVALID_ARGS);
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
    context.set_error_handler([&run_loop](zx_status_t status) {
      run_loop.Quit();
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
    context.set_error_handler([&run_loop](zx_status_t status) {
      run_loop.Quit();
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
    context.set_error_handler([&run_loop](zx_status_t status) {
      run_loop.Quit();
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
  ASSERT_EQ(
      base::WriteFile(profile_temp_dir.GetPath().AppendASCII(kTestDataFileIn),
                      nullptr, 0),
      0);

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

static bool WaitUntilJobIsEmpty(zx::unowned_job job, zx::duration timeout) {
  zx_signals_t observed = 0;
  zx_status_t status =
      job->wait_one(ZX_JOB_NO_JOBS, zx::deadline_after(timeout), &observed);
  ZX_CHECK(status == ZX_OK || status == ZX_ERR_TIMED_OUT, status);
  return observed & ZX_JOB_NO_JOBS;
}

// Regression test for https://crbug.com/927403 (Job leak per-Context).
TEST_F(ContextProviderImplTest, CleansUpContextJobs) {
  // Replace the default job with one that is guaranteed to be empty.
  zx::job job;
  ASSERT_EQ(zx::job::create(*base::GetDefaultJob(), 0, &job), ZX_OK);
  base::ScopedDefaultJobForTest empty_default_job(std::move(job));

  // Bind to the ContextProvider.
  fuchsia::web::ContextProviderPtr provider;
  bindings_.AddBinding(provider_.get(), provider.NewRequest());

  // Verify that our current default job is still empty.
  ASSERT_TRUE(WaitUntilJobIsEmpty(base::GetDefaultJob(), zx::duration()));

  // Create a Context and verify that it is functional.
  fuchsia::web::ContextPtr context;
  provider->Create(BuildCreateContextParams(), context.NewRequest());
  CheckContextResponsive(&context);

  // Verify that there is at least one job under our default job.
  ASSERT_FALSE(WaitUntilJobIsEmpty(base::GetDefaultJob(), zx::duration()));

  // Detach from the Context and ContextProvider, and spin the loop to allow
  // those to be handled.
  context.Unbind();
  provider.Unbind();
  base::RunLoop().RunUntilIdle();

  // Wait until the default job signals that it no longer contains any child
  // jobs; this should occur shortly after the Context process terminates.
  EXPECT_TRUE(WaitUntilJobIsEmpty(
      base::GetDefaultJob(),
      zx::duration(TestTimeouts::action_timeout().InNanoseconds())));
}

TEST(ContextProviderImplConfigTest, WithConfigWithCommandLineArgs) {
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Specify a configuration that sets valid args with valid strings.
  base::Value config_dict =
      CreateConfigWithSwitchValue("renderer-process-limit", "0");

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.set_config_for_test(std::move(config_dict));
  context_provider.SetLaunchCallbackForTest(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command,
                                         const base::LaunchOptions& options) {
        loop.Quit();
        EXPECT_TRUE(command.HasSwitch("renderer-process-limit"));
        return base::Process();
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });
  context_provider.Create(BuildCreateContextParams(), context.NewRequest());

  loop.Run();
}

TEST(ContextProviderImplConfigTest, WithConfigWithDisallowedCommandLineArgs) {
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  // Specify a configuration that sets a disallowed command-line argument.
  base::Value config_dict =
      CreateConfigWithSwitchValue("kittens-are-nice", "0");

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.set_config_for_test(std::move(config_dict));
  context_provider.SetLaunchCallbackForTest(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command,
                                         const base::LaunchOptions& options) {
        loop.Quit();
        EXPECT_FALSE(command.HasSwitch("kittens-are-nice"));
        return base::Process();
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });
  context_provider.Create(BuildCreateContextParams(), context.NewRequest());

  loop.Run();
}

TEST(ContextProviderImplConfigTest, WithConfigWithWronglyTypedCommandLineArgs) {
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::Value config_dict(base::Value::Type::DICTIONARY);

  // Specify a configuration that sets valid args with invalid value.
  base::Value args(base::Value::Type::DICTIONARY);
  args.SetBoolKey("renderer-process-limit", false);
  config_dict.SetKey(kCommandLineArgs, std::move(args));

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.set_config_for_test(std::move(config_dict));
  context_provider.SetLaunchCallbackForTest(
      base::BindLambdaForTesting([&](const base::CommandLine& command,
                                     const base::LaunchOptions& options) {
        loop.Quit();
        ADD_FAILURE();
        return base::Process();
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    EXPECT_EQ(status, ZX_ERR_INTERNAL);
  });
  context_provider.Create(BuildCreateContextParams(), context.NewRequest());

  loop.Run();
}

// Tests that unsafely_treat_insecure_origins_as_secure properly adds the right
// command-line arguments to the Context process.
TEST(ContextProviderImplParamsTest, WithInsecureOriginsAsSecure) {
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.SetLaunchCallbackForTest(
      base::BindLambdaForTesting([&](const base::CommandLine& command,
                                     const base::LaunchOptions& options) {
        loop.Quit();
        EXPECT_TRUE(command.HasSwitch(switches::kAllowRunningInsecureContent));
        EXPECT_THAT(command.GetSwitchValueASCII(switches::kDisableFeatures),
                    testing::HasSubstr("AutoupgradeMixedContent"));
        EXPECT_EQ(command.GetSwitchValueASCII(
                      network::switches::kUnsafelyTreatInsecureOriginAsSecure),
                  "http://example.com");
        return base::Process();
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  std::vector<std::string> insecure_origins;
  insecure_origins.push_back(switches::kAllowRunningInsecureContent);
  insecure_origins.push_back("disable-mixed-content-autoupgrade");
  insecure_origins.push_back("http://example.com");
  create_params.set_unsafely_treat_insecure_origins_as_secure(
      std::move(insecure_origins));
  context_provider.Create(std::move(create_params), context.NewRequest());

  loop.Run();
}

TEST(ContextProviderImplConfigTest, WithDataQuotaBytes) {
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.SetLaunchCallbackForTest(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command,
                                         const base::LaunchOptions& options) {
        loop.Quit();
        EXPECT_EQ(command.GetSwitchValueASCII("data-quota-bytes"),
                  kTestQuotaBytesSwitchValue);
        return base::Process();
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
  context_provider.Create(std::move(create_params), context.NewRequest());

  loop.Run();
}

TEST(ContextProviderImplConfigTest, WithGoogleApiKeyValue) {
  constexpr char kDummyApiKey[] = "apikey123";
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::Value config_dict =
      CreateConfigWithSwitchValue("google-api-key", kDummyApiKey);

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.set_config_for_test(std::move(config_dict));
  context_provider.SetLaunchCallbackForTest(base::BindLambdaForTesting(
      [&loop, kDummyApiKey](const base::CommandLine& command,
                            const base::LaunchOptions& options) {
        loop.Quit();
        EXPECT_EQ(command.GetSwitchValueASCII(switches::kGoogleApiKey),
                  kDummyApiKey);
        return base::Process();
      }));

  fuchsia::web::ContextPtr context;
  context.set_error_handler([&loop](zx_status_t status) {
    loop.Quit();
    ZX_LOG(ERROR, status);
    ADD_FAILURE();
  });
  context_provider.Create(BuildCreateContextParams(), context.NewRequest());

  loop.Run();
}

// TODO(crbug.com/1013412): This test doesn't actually exercise DRM, so could
// be executed everywhere if DRM support were configurable.
#if defined(ARCH_CPU_ARM64)
TEST(ContextProviderImplConfigTest, WithCdmDataQuotaBytes) {
  const base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  base::RunLoop loop;
  ContextProviderImpl context_provider;
  context_provider.SetLaunchCallbackForTest(
      base::BindLambdaForTesting([&loop](const base::CommandLine& command,
                                         const base::LaunchOptions& options) {
        loop.Quit();
        EXPECT_EQ(command.GetSwitchValueASCII("cdm-data-quota-bytes"),
                  kTestQuotaBytesSwitchValue);
        return base::Process();
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
  context_provider.Create(std::move(create_params), context.NewRequest());

  loop.Run();
}
#endif  // defined(ARCH_CPU_ARM64)
