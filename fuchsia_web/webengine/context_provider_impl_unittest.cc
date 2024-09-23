// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/context_provider_impl.h"

#include <fuchsia/web/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fdio/namespace.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/service.h>
#include <zircon/status.h>
#include <zircon/types.h>

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump_type.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/fuchsia_component_support/append_arguments_from_file.h"
#include "components/fuchsia_component_support/mock_realm.h"
#include "fuchsia_web/webengine/switches.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using ::testing::_;
using ::testing::Not;

constexpr char kTestDataFileIn[] = "DataFileIn";
constexpr char kTestDataFileOut[] = "DataFileOut";

constexpr uint64_t kTestQuotaBytes = 1024;
constexpr char kTestQuotaBytesSwitchValue[] = "1024";

// A fake implementation of fuchsia.component/Realm.
class FakeRealm {
 public:
  class Delegate {
   public:
    virtual ~Delegate() = default;
    virtual void OnChildInstanceCreated(const std::string& name) = 0;

   protected:
    Delegate() = default;
  };

  explicit FakeRealm(Delegate& delegate) : delegate_(delegate) {}
  FakeRealm(const FakeRealm&) = delete;
  FakeRealm& operator=(const FakeRealm&) = delete;
  ~FakeRealm() = default;

  // Returns true if the realm has no child instances.
  bool empty() const { return instances_.empty(); }

  // Sets a callback that will be run when the realm's last child is deleted.
  void set_on_empty_callback(base::OnceClosure on_empty_callback) {
    on_empty_callback_ = std::move(on_empty_callback);
  }

  // Saves copies of `child_decl` and `args` for subsequent validation by tests.
  void CreateChild(const fuchsia::component::decl::Child& child_decl,
                   const fuchsia::component::CreateChildArgs& args) {
    ASSERT_TRUE(!base::Contains(instances_, child_decl.name()));

    auto& child = instances_[child_decl.name()];
    child.decl = std::make_unique<fuchsia::component::decl::Child>();
    ASSERT_EQ(child_decl.Clone(child.decl.get()), ZX_OK);
    child.args = std::make_unique<fuchsia::component::CreateChildArgs>();
    ASSERT_EQ(args.Clone(child.args.get()), ZX_OK);
  }

  // Satisfies a request for `child_ref`'s dir by serving a PseudoDir on the
  // current thread containing entries for the Binder and Context protocols.
  // When the client connects to the last of these protocols, the realm
  // delegate's `OnChildInstanceCreated` method is called with the child's name.
  void OpenExposedDir(
      fuchsia::component::decl::ChildRef child_ref,
      fidl::InterfaceRequest<fuchsia::io::Directory> exposed_dir,
      fidl::InterfaceRequest<fuchsia::component::Binder>& binder_request,
      fidl::InterfaceRequest<fuchsia::web::Context>& context_request) {
    auto it = instances_.find(child_ref.name);
    ASSERT_FALSE(it == instances_.end());

    auto& child = it->second;

    // Publish a fake Binder to the child's exposed directory to capture the
    // host's request so that the test may close it to trigger child
    // destruction.
    child.instances.AddEntry(
        fuchsia::component::Binder::Name_,
        std::make_unique<vfs::Service>([&binder_request](zx::channel request,
                                                         async_dispatcher_t*) {
          binder_request = fidl::InterfaceRequest<fuchsia::component::Binder>(
              std::move(request));
        }));

    // Publish a fake Context that triggers notifying the delegate.
    child.instances.AddEntry(
        fuchsia::web::Context::Name_,
        std::make_unique<vfs::Service>(
            [&context_request, delegate = delegate_, name = child_ref.name](
                zx::channel request, async_dispatcher_t*) {
              context_request = fidl::InterfaceRequest<fuchsia::web::Context>(
                  std::move(request));
              delegate->OnChildInstanceCreated(name);
            }));

    child.instances.Serve(fuchsia::io::OpenFlags::RIGHT_READABLE |
                              fuchsia::io::OpenFlags::RIGHT_WRITABLE,
                          exposed_dir.TakeChannel());
  }

  // Destroys the child and runs the `on_empty_callback` if none remain.
  void DestroyChild(fuchsia::component::decl::ChildRef child) {
    ASSERT_EQ(instances_.erase(child.name), 1u);
    if (instances_.empty() && on_empty_callback_) {
      std::move(on_empty_callback_).Run();
    }
  }

  // Returns true if the realm has a child with the same name as `child`.
  bool HasChild(const fuchsia::component::decl::Child& child) {
    return base::Contains(instances_, child.name());
  }

  // Returns true if the realm has a child with the same name as `child`.
  bool HasChild(const fuchsia::component::decl::ChildRef& child) {
    return base::Contains(instances_, child.name);
  }

  // Returns the component declaration used to create the child named `name`.
  const fuchsia::component::decl::Child& GetChildDecl(const std::string& name) {
    return *instances_[name].decl;
  }

  // Returns the args used to create the child named `name`.
  const fuchsia::component::CreateChildArgs& GetChildArgs(
      const std::string& name) {
    return *instances_[name].args;
  }

 private:
  struct Child {
    vfs::PseudoDir instances;
    std::unique_ptr<fuchsia::component::decl::Child> decl;
    std::unique_ptr<fuchsia::component::CreateChildArgs> args;
  };

  const raw_ref<Delegate> delegate_;
  std::map<std::string, Child> instances_;
  base::OnceClosure on_empty_callback_;
};

class MockFakeRealmDelegate : public FakeRealm::Delegate {
 public:
  MOCK_METHOD(void,
              OnChildInstanceCreated,
              (const std::string& name),
              (override));
};

// Returns true if `arg` (a fuchsia::component::decl::CollectionRef) references
// `name`.
MATCHER_P(CollectionNameIs, name, "") {
  return arg.name == name;
}

// Returns true if the URL of `arg` (a fuchsia::component::decl::Child) is
// `url`.
MATCHER_P(UrlIs, url, "") {
  return arg.has_url() && arg.url() == url;
}

// Returns true if `realm` knows `arg`.
MATCHER_P(IsInRealm, realm, "") {
  return realm->HasChild(arg);
}

// Returns true if `arg` (a fuchsia::component::CreateChildArgs) has a dynamic
// directory offer for `name`.
MATCHER_P2(HasDynamicDirectoryOffer, name, rights, "") {
  if (!arg.has_dynamic_offers()) {
    return false;
  }
  for (const auto& offer : arg.dynamic_offers()) {
    if (offer.is_directory() && offer.directory().has_target_name() &&
        offer.directory().target_name() == name &&
        offer.directory().has_rights() &&
        offer.directory().rights() == rights) {
      return true;
    }
  }
  return false;
}

// Returns a primitive `CreateContextParams` configured to pass the test
// component's service directory as the service directory to be used by the
// context.
fuchsia::web::CreateContextParams BuildCreateContextParams() {
  fuchsia::web::CreateContextParams output;
  zx_status_t result = fdio_service_connect(
      base::kServiceDirectoryPath,
      output.mutable_service_directory()->NewRequest().TakeChannel().release());
  EXPECT_EQ(result, ZX_OK) << "Failed to open /svc";
  return output;
}

// Returns a handle to the test component's `/cache` directory.
fidl::InterfaceHandle<fuchsia::io::Directory> OpenCacheDirectory() {
  fidl::InterfaceHandle<fuchsia::io::Directory> cache_handle;
  zx_status_t result =
      fdio_service_connect(base::kPersistedCacheDirectoryPath,
                           cache_handle.NewRequest().TakeChannel().release());
  EXPECT_EQ(result, ZX_OK) << "Failed to open /cache";
  return cache_handle;
}

}  // namespace

class ContextProviderImplTest : public ::testing::Test {
 protected:
  ContextProviderImplTest()
      : service_thread_("outgoing server"),
        mock_realm_(test_component_context_.additional_services()) {
    provider_.emplace(outgoing_directory_);
    bindings_.AddBinding(&provider_.value(), provider_ptr_.NewRequest());
  }

  static void SetUpTestSuite() {
    // Need to do this once on the main thread before spinning off others.
    base::PlatformThread::SetCurrentThreadType(base::ThreadType::kDefault);
  }

  void SetUp() override {
    // Serve the context provider's outgoing directory on the service thread.
    ASSERT_TRUE(
        service_thread_.StartWithOptions({base::MessagePumpType::IO, 0}));
    fidl::InterfaceHandle<fuchsia::io::Directory> handle;
    service_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](sys::OutgoingDirectory* dir,
               fidl::InterfaceRequest<fuchsia::io::Directory> request) {
              dir->Serve(std::move(request));
            },
            base::Unretained(&outgoing_directory_), handle.NewRequest()));
    service_thread_.FlushForTesting();

    // Install the outgoing directory in the test's namespace for inspection.
    fdio_ns_t* ns = nullptr;
    ASSERT_EQ(::fdio_ns_get_installed(&ns), ZX_OK);
    global_namespace_ = ns;
    ASSERT_EQ(::fdio_ns_bind(global_namespace_, kTestOutgoingPath,
                             handle.TakeChannel().release()),
              ZX_OK);
  }

  void TearDown() override {
    // Shut down the ContextProvider.
    provider_ptr_.Unbind();
    provider_.reset();

    // Wait for all children to be destroyed before the mock is destroyed.
    if (!fake_realm_.empty()) {
      base::RunLoop run_loop;
      fake_realm_.set_on_empty_callback(run_loop.QuitClosure());
      run_loop.Run();
    }

    ASSERT_EQ(::fdio_ns_unbind(global_namespace_, kTestOutgoingPath), ZX_OK);
  }

  // Add expectations to the MockRealm for creation and destruction of a single
  // child web_instance, invoking the respective methods on the test's
  // FakeRealm. `binder_request` and `context_request` capture the request
  // channels for the Binder and Context protocols, respectively.
  void ExpectChildInstance(
      fidl::InterfaceRequest<fuchsia::component::Binder>& binder_request,
      fidl::InterfaceRequest<fuchsia::web::Context>& context_request) {
    ::testing::InSequence sequence;

    EXPECT_CALL(mock_realm_, CreateChild(CollectionNameIs("web_instances"),
                                         Not(IsInRealm(&fake_realm_)), _, _))
        .WillOnce([this](const auto& collection, const auto& decl,
                         const auto& args, auto callback) {
          fake_realm_.CreateChild(decl, args);
          callback(
              fuchsia::component::Realm_CreateChild_Result::WithResponse({}));
        })
        .RetiresOnSaturation();

    EXPECT_CALL(mock_realm_, OpenExposedDir(IsInRealm(&fake_realm_), _, _))
        .WillOnce([this, &binder_request, &context_request](
                      const auto& child, auto dir_request, auto callback) {
          fake_realm_.OpenExposedDir(child, std::move(dir_request),
                                     binder_request, context_request);
          callback(
              fuchsia::component::Realm_OpenExposedDir_Result::WithResponse(
                  {}));
        })
        .RetiresOnSaturation();

    EXPECT_CALL(mock_realm_, DestroyChild(IsInRealm(&fake_realm_), _))
        .WillOnce([this](const auto& child, auto callback) {
          fake_realm_.DestroyChild(child);
          callback(
              fuchsia::component::Realm_DestroyChild_Result::WithResponse({}));
        })
        .RetiresOnSaturation();
  }

  // Calls on `provider` to create a new web instance and waits for its
  // creation. Returns the name of the newly-created instance.
  std::string CreateAndWaitForInstance(
      fuchsia::web::ContextProvider& provider,
      fuchsia::web::CreateContextParams create_params,
      fuchsia::web::ContextPtr& context_ptr) {
    base::RunLoop run_loop;

    // Terminate the loop if there's an error reported via the Context pointer.
    context_ptr.set_error_handler(
        [quit_loop = run_loop.QuitClosure()](zx_status_t error_status) {
          ADD_FAILURE() << "Context unexpectedly closed while waiting for "
                           "instance: "
                        << zx_status_get_string(error_status);
          quit_loop.Run();
        });

    provider.Create(std::move(create_params), context_ptr.NewRequest());

    // Pump events until the child instance is created and capture its name.
    std::string instance_name;
    EXPECT_CALL(mock_fake_realm_delegate_, OnChildInstanceCreated(_))
        .WillOnce([&instance_name, quit_loop = run_loop.QuitClosure()](
                      const std::string& name) {
          instance_name = name;
          quit_loop.Run();
        });
    run_loop.Run();
    ::testing::Mock::VerifyAndClearExpectations(&mock_fake_realm_delegate_);

    // Clear the error handler since the loop is no longer running.
    context_ptr.set_error_handler({});

    return instance_name;
  }

  // Waits for the Context channel to be closed and returns the associated
  // error or Epitaph.
  zx_status_t WaitForContextClosedStatus(
      fidl::InterfacePtr<fuchsia::web::Context>& context) {
    zx_status_t status = ZX_OK;
    base::RunLoop run_loop;
    context.set_error_handler(
        [&status, quit_loop = run_loop.QuitClosure()](zx_status_t error) {
          status = error;
          quit_loop.Run();
        });
    run_loop.Run();
    return status;
  }

  // Returns the component declaration used to create the child named `name`.
  const fuchsia::component::decl::Child& GetInstanceDecl(
      const std::string& name) {
    return fake_realm_.GetChildDecl(name);
  }

  // Returns the args used to create the child named `name`.
  const fuchsia::component::CreateChildArgs& GetInstanceArgs(
      const std::string& name) {
    return fake_realm_.GetChildArgs(name);
  }

  // Returns the path to the directory in the test's namespace holding dynamic
  // directory offers for the named instance.
  static base::FilePath GetInstanceDirectory(const std::string& name) {
    return base::FilePath(kTestOutgoingPath)
        .AppendASCII("web_instances")
        .AppendASCII(name);
  }

  // Returns the command line passed to the named instance.
  static base::CommandLine GetInstanceCommandLine(const std::string& name) {
    base::CommandLine command_line(base::CommandLine::NO_PROGRAM);
    EXPECT_TRUE(fuchsia_component_support::AppendArgumentsFromFile(
        GetInstanceDirectory(name)
            .AppendASCII("command-line-config")
            .AppendASCII("argv.json"),
        command_line));
    return command_line;
  }

  fuchsia::web::ContextProvider& context_provider() { return *provider_ptr_; }

  void BindContextProvider(
      fidl::InterfaceRequest<fuchsia::web::ContextProvider> request) {
    bindings_.AddBinding(&provider_.value(), std::move(request));
  }

 private:
  // The path in the test's namespace where the outgoing directory given to
  // ContextProvider is bound for the sake of analysis.
  static constexpr char kTestOutgoingPath[] = "/test_outgoing_path";

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};

  raw_ptr<fdio_ns_t> global_namespace_ = nullptr;
  sys::OutgoingDirectory outgoing_directory_;
  // The thread serving `outgoing_directory_` must be destroyed before the
  // directory itself.
  base::Thread service_thread_;

  ::testing::StrictMock<MockFakeRealmDelegate> mock_fake_realm_delegate_;
  FakeRealm fake_realm_{mock_fake_realm_delegate_};

  // Used to replace the process component context with one providing a fake
  // fuchsia.component.Realm.
  base::TestComponentContextForProcess test_component_context_;

  // A mock fuchsia::component/Realm used to bridge to `fake_realm_`.
  ::testing::StrictMock<fuchsia_component_support::MockRealm> mock_realm_;
  fidl::BindingSet<fuchsia::web::ContextProvider> bindings_;
  std::optional<ContextProviderImpl> provider_;
  fuchsia::web::ContextProviderPtr provider_ptr_;
};

TEST_F(ContextProviderImplTest, CanCreateContextWithServiceDirectory) {
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request;
  ExpectChildInstance(binder_request, context_request);

  fuchsia::web::ContextPtr context;
  const std::string instance_name = CreateAndWaitForInstance(
      context_provider(), BuildCreateContextParams(), context);
  ASSERT_FALSE(instance_name.empty());

  // Requests for both interfaces should have been made.
  ASSERT_TRUE(binder_request);
  ASSERT_TRUE(context_request);

  const auto& child = GetInstanceDecl(instance_name);
  const auto& create_child_args = GetInstanceArgs(instance_name);

  ASSERT_THAT(child, UrlIs("#meta/web_instance_with_svc_directory.cm"));
  ASSERT_THAT(
      create_child_args,
      HasDynamicDirectoryOffer("svc", fuchsia::io::Operations::CONNECT |
                                          fuchsia::io::Operations::ENUMERATE |
                                          fuchsia::io::Operations::TRAVERSE));
  ASSERT_PRED1(base::PathExists,
               GetInstanceDirectory(instance_name).AppendASCII("svc"));
}

TEST_F(ContextProviderImplTest, CreateContextWithoutServiceDirectoryFails) {
  fuchsia::web::ContextPtr context_ptr;
  context_provider().Create({}, context_ptr.NewRequest());
  ASSERT_EQ(WaitForContextClosedStatus(context_ptr), ZX_ERR_INVALID_ARGS);
}

TEST_F(ContextProviderImplTest, CreateValidatesDataDirectory) {
  // Deliberately supply the wrong kind of object as the data-directory.
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  zx::socket socket1, socket2;
  ASSERT_EQ(zx::socket::create(0, &socket1, &socket2), ZX_OK);
  create_params.set_data_directory(
      fidl::InterfaceHandle<fuchsia::io::Directory>(
          zx::channel(socket1.release())));

  fidl::InterfacePtr<fuchsia::web::Context> context_ptr;
  context_provider().Create(std::move(create_params), context_ptr.NewRequest());
  ASSERT_EQ(WaitForContextClosedStatus(context_ptr), ZX_ERR_PEER_CLOSED);
}

// Request Widevine DRM but do not enable VULKAN.
TEST_F(ContextProviderImplTest, CreateValidatesWidevineWithoutVulkan) {
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  *create_params.mutable_features() =
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM;
  *create_params.mutable_cdm_data_directory() = OpenCacheDirectory();

  fidl::InterfacePtr<fuchsia::web::Context> context;
  context_provider().Create(std::move(create_params), context.NewRequest());
  ASSERT_EQ(WaitForContextClosedStatus(context), ZX_ERR_NOT_SUPPORTED);
}

// Request PlayReady DRM but do not enable VULKAN.
TEST_F(ContextProviderImplTest, CreateValidatesPlayReadyWithoutVulkan) {
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  create_params.set_playready_key_system("foo");
  *create_params.mutable_cdm_data_directory() = OpenCacheDirectory();

  fidl::InterfacePtr<fuchsia::web::Context> context;
  context_provider().Create(std::move(create_params), context.NewRequest());
  ASSERT_EQ(WaitForContextClosedStatus(context), ZX_ERR_NOT_SUPPORTED);
}

// Requesting DRM without VULKAN is acceptable for HEADLESS Contexts.
TEST_F(ContextProviderImplTest, CreateHeadlessDrmWithoutVulkan) {
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request;
  ExpectChildInstance(binder_request, context_request);

  fuchsia::web::ContextPtr context;
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  *create_params.mutable_features() =
      fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM |
      fuchsia::web::ContextFeatureFlags::HEADLESS;
  *create_params.mutable_cdm_data_directory() = OpenCacheDirectory();
  const std::string instance_name = CreateAndWaitForInstance(
      context_provider(), std::move(create_params), context);
  ASSERT_FALSE(instance_name.empty());

  // Requests for both interfaces should have been made.
  ASSERT_TRUE(binder_request);
  ASSERT_TRUE(context_request);

  const auto& child = GetInstanceDecl(instance_name);
  const auto& create_child_args = GetInstanceArgs(instance_name);

  ASSERT_THAT(child, UrlIs("#meta/web_instance_with_svc_directory.cm"));
  ASSERT_THAT(create_child_args,
              HasDynamicDirectoryOffer("cdm_data", fuchsia::io::RW_STAR_DIR));
  ASSERT_THAT(
      create_child_args,
      HasDynamicDirectoryOffer("svc", fuchsia::io::Operations::CONNECT |
                                          fuchsia::io::Operations::ENUMERATE |
                                          fuchsia::io::Operations::TRAVERSE));
  ASSERT_PRED1(base::PathExists,
               GetInstanceDirectory(instance_name).AppendASCII("cdm_data"));
  ASSERT_PRED1(base::PathExists,
               GetInstanceDirectory(instance_name).AppendASCII("svc"));
}

TEST_F(ContextProviderImplTest, MultipleConcurrentClients) {
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request_1;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request_1;
  ExpectChildInstance(binder_request_1, context_request_1);
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request_2;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request_2;
  ExpectChildInstance(binder_request_2, context_request_2);
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request_3;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request_3;
  ExpectChildInstance(binder_request_3, context_request_3);

  // Create a Context via the pre-bound Provider pointer.
  fuchsia::web::ContextPtr context_1;
  const std::string instance_1_name = CreateAndWaitForInstance(
      context_provider(), BuildCreateContextParams(), context_1);
  ASSERT_FALSE(instance_1_name.empty());

  // Bind a second Provider pointer and create a Context via it.
  fuchsia::web::ContextProviderPtr provider_2_ptr;
  BindContextProvider(provider_2_ptr.NewRequest());
  fuchsia::web::ContextPtr context_2;
  const std::string instance_2_name = CreateAndWaitForInstance(
      *provider_2_ptr, BuildCreateContextParams(), context_2);
  ASSERT_FALSE(instance_2_name.empty());

  // Ensure that the initial ContextProvider connection is still usable, by
  // creating and verifying another Context from it.
  fuchsia::web::ContextPtr context_3;
  const std::string instance_3_name = CreateAndWaitForInstance(
      context_provider(), BuildCreateContextParams(), context_3);
  ASSERT_FALSE(instance_3_name.empty());
}

TEST_F(ContextProviderImplTest, WithProfileDir) {
  base::ScopedTempDir profile_temp_dir;

  // Setup data dir.
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());
  ASSERT_TRUE(
      base::WriteFile(profile_temp_dir.GetPath().AppendASCII(kTestDataFileIn),
                      std::string_view()));

  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request;
  ExpectChildInstance(binder_request, context_request);

  // Pass a handle data dir to the context.
  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  create_params.set_data_directory(
      base::OpenDirectoryHandle(profile_temp_dir.GetPath()));

  fuchsia::web::ContextPtr context;
  const std::string instance_name = CreateAndWaitForInstance(
      context_provider(), std::move(create_params), context);
  ASSERT_FALSE(instance_name.empty());

  // Requests for both interfaces should have been made.
  ASSERT_TRUE(binder_request);
  ASSERT_TRUE(context_request);

  const auto& child = GetInstanceDecl(instance_name);
  const auto& create_child_args = GetInstanceArgs(instance_name);
  const base::CommandLine command = GetInstanceCommandLine(instance_name);

  ASSERT_THAT(child, UrlIs("#meta/web_instance_with_svc_directory.cm"));
  ASSERT_THAT(create_child_args,
              HasDynamicDirectoryOffer("data", fuchsia::io::RW_STAR_DIR));
  EXPECT_FALSE(command.HasSwitch(switches::kIncognito));

  auto data_dir = GetInstanceDirectory(instance_name).AppendASCII("data");
  ASSERT_PRED1(base::PathExists, data_dir);

  // Make sure the input file can be seen in the mounted dir.
  ASSERT_PRED1(base::PathExists, data_dir.AppendASCII(kTestDataFileIn));

  // Make sure that the mapped dir can be written to.
  ASSERT_TRUE(base::WriteFile(data_dir.AppendASCII(kTestDataFileOut),
                              std::string_view()));
  ASSERT_PRED1(base::PathExists,
               profile_temp_dir.GetPath().AppendASCII(kTestDataFileOut));
}

// Verify that creation fails when passing in a file rather than a directory.
TEST_F(ContextProviderImplTest, FailsDataDirectoryIsFile) {
  base::ScopedTempDir profile_temp_dir;
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());

  base::FilePath temp_file_path;
  ASSERT_TRUE(base::CreateTemporaryFileInDir(profile_temp_dir.GetPath(),
                                             &temp_file_path));

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  create_params.set_data_directory(base::OpenDirectoryHandle(temp_file_path));

  fidl::InterfacePtr<fuchsia::web::Context> context;
  context_provider().Create(std::move(create_params), context.NewRequest());
  ASSERT_EQ(WaitForContextClosedStatus(context), ZX_ERR_PEER_CLOSED);
}

// Tests that unsafely_treat_insecure_origins_as_secure properly adds the right
// command-line arguments to the Context process.
TEST_F(ContextProviderImplTest, WithInsecureOriginsAsSecure) {
  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request;
  ExpectChildInstance(binder_request, context_request);

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  create_params.set_unsafely_treat_insecure_origins_as_secure(
      {"allow-running-insecure-content", "disable-mixed-content-autoupgrade",
       "http://example.com", "http://example.net"});

  fuchsia::web::ContextPtr context;
  const std::string instance_name = CreateAndWaitForInstance(
      context_provider(), std::move(create_params), context);
  ASSERT_FALSE(instance_name.empty());

  // Requests for both interfaces should have been made.
  ASSERT_TRUE(binder_request);
  ASSERT_TRUE(context_request);

  static constexpr char kAllowRunningInsecureContent[] =
      "allow-running-insecure-content";

  const base::CommandLine command = GetInstanceCommandLine(instance_name);

  EXPECT_TRUE(command.HasSwitch(
      network::switches::kUnsafelyTreatInsecureOriginAsSecure));
#if BUILDFLAG(ENABLE_CAST_RECEIVER)
  ASSERT_STREQ(kAllowRunningInsecureContent,
               switches::kAllowRunningInsecureContent);
  EXPECT_TRUE(command.HasSwitch(kAllowRunningInsecureContent));
  EXPECT_THAT(command.GetSwitchValueASCII(switches::kDisableFeatures),
              ::testing::HasSubstr("AutoupgradeMixedContent"));
  EXPECT_EQ(command.GetSwitchValueASCII(
                network::switches::kUnsafelyTreatInsecureOriginAsSecure),
            "http://example.com,http://example.net");
#else   // BUILDFLAG(ENABLE_CAST_RECEIVER)
  EXPECT_FALSE(command.HasSwitch(kAllowRunningInsecureContent));
  EXPECT_FALSE(command.HasSwitch(switches::kDisableFeatures));

  // The unrecognized values are passed on as origins.
  EXPECT_EQ(command.GetSwitchValueASCII(
                network::switches::kUnsafelyTreatInsecureOriginAsSecure),
            "allow-running-insecure-content,"
            "disable-mixed-content-autoupgrade,"
            "http://example.com,http://example.net");
#endif  // BUILDFLAG(ENABLE_CAST_RECEIVER)
}

TEST_F(ContextProviderImplTest, WithDataQuotaBytes) {
  base::ScopedTempDir profile_temp_dir;
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());

  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request;
  ExpectChildInstance(binder_request, context_request);

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  create_params.set_data_directory(
      base::OpenDirectoryHandle(profile_temp_dir.GetPath()));
  create_params.set_data_quota_bytes(kTestQuotaBytes);

  fuchsia::web::ContextPtr context;
  const std::string instance_name = CreateAndWaitForInstance(
      context_provider(), std::move(create_params), context);
  ASSERT_FALSE(instance_name.empty());

  // Requests for both interfaces should have been made.
  ASSERT_TRUE(binder_request);
  ASSERT_TRUE(context_request);

  const base::CommandLine command = GetInstanceCommandLine(instance_name);
  EXPECT_EQ(command.GetSwitchValueASCII("data-quota-bytes"),
            kTestQuotaBytesSwitchValue);
}

TEST_F(ContextProviderImplTest, WithCdmDataQuotaBytes) {
  base::ScopedTempDir profile_temp_dir;
  ASSERT_TRUE(profile_temp_dir.CreateUniqueTempDir());

  fidl::InterfaceRequest<fuchsia::component::Binder> binder_request;
  fidl::InterfaceRequest<fuchsia::web::Context> context_request;
  ExpectChildInstance(binder_request, context_request);

  fuchsia::web::CreateContextParams create_params = BuildCreateContextParams();
  create_params.set_cdm_data_directory(
      base::OpenDirectoryHandle(profile_temp_dir.GetPath()));
  create_params.set_features(fuchsia::web::ContextFeatureFlags::HEADLESS |
                             fuchsia::web::ContextFeatureFlags::WIDEVINE_CDM);
  create_params.set_cdm_data_quota_bytes(kTestQuotaBytes);

  fuchsia::web::ContextPtr context;
  const std::string instance_name = CreateAndWaitForInstance(
      context_provider(), std::move(create_params), context);
  ASSERT_FALSE(instance_name.empty());

  // Requests for both interfaces should have been made.
  ASSERT_TRUE(binder_request);
  ASSERT_TRUE(context_request);

  const base::CommandLine command = GetInstanceCommandLine(instance_name);
  EXPECT_EQ(command.GetSwitchValueASCII("cdm-data-quota-bytes"),
            kTestQuotaBytesSwitchValue);
}
