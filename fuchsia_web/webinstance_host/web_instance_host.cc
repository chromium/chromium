// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webinstance_host/web_instance_host.h"

#include <fuchsia/component/decl/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/pseudo_file.h>
#include <lib/vfs/cpp/remote_dir.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/types/expected.h"
#include "components/fuchsia_component_support/serialize_arguments.h"
#include "fuchsia_web/webengine/switches.h"
#include "fuchsia_web/webinstance_host/web_instance_host_internal.h"

namespace {

namespace fcdecl = ::fuchsia::component::decl;

// Production URL for web hosting Component instances.
// The URL cannot be obtained programmatically - see fxbug.dev/51490.
constexpr char kWebInstanceComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine#meta/web_instance.cm";

// Test-only URL for web hosting Component instances with WebUI resources.
const char kWebInstanceWithWebUiComponentUrl[] =
    "fuchsia-pkg://fuchsia.com/web_engine_with_webui#meta/web_instance.cm";

// The name of the component collection hosting the instances.
constexpr char kCollectionName[] = "web_instances";

// Returns the "/web_instances" dir from the component's outgoing directory,
// creating it if necessary.
vfs::PseudoDir* GetWebInstancesCollectionDir() {
  return base::ComponentContextForProcess()->outgoing()->GetOrCreateDirectory(
      kCollectionName);
}

// Returns an instance's name given its unique id.
std::string InstanceNameFromId(const base::GUID& id) {
  return base::StrCat({kCollectionName, "_", id.AsLowercaseString()});
}

void DestroyInstance(fuchsia::component::Realm& realm,
                     const std::string& name) {
  realm.DestroyChild(
      fcdecl::ChildRef{.name = name, .collection = kCollectionName},
      [](::fuchsia::component::Realm_DestroyChild_Result destroy_result) {
        DCHECK(!destroy_result.is_err())
            << static_cast<uint32_t>(destroy_result.err());
      });
}

void DestroyInstanceDirectory(vfs::PseudoDir* instances_dir,
                              const std::string& name) {
  zx_status_t status = instances_dir->RemoveEntry(name);
  ZX_DCHECK(status == ZX_OK, status);
}

struct Instance {
  base::GUID id;
  fuchsia::component::BinderPtr binder_ptr;

  Instance(base::GUID id, fuchsia::component::BinderPtr binder_ptr)
      : id(std::move(id)), binder_ptr(std::move(binder_ptr)) {}
};

// A helper class for building a web_instance as a dynamic child of the
// component that hosts WebInstanceHost.
class InstanceBuilder {
 public:
  static base::expected<std::unique_ptr<InstanceBuilder>, zx_status_t> Create(
      fuchsia::component::Realm& realm,
      const base::CommandLine& launch_args);
  ~InstanceBuilder();

  base::CommandLine& args() { return args_; }

  // Offers the services named in `services` to the instance as dynamic
  // protocol offers.
  void AppendOffersForServices(const std::vector<std::string>& services);

  // Serves `data_directory` to the instance as the `data` read-write
  // directory.
  void ServeDataDirectory(
      fidl::InterfaceHandle<fuchsia::io::Directory> data_directory);

  // Serves the directories in `providers` under the `content-directories`
  // read-only directory.
  zx_status_t ServeContentDirectories(
      std::vector<fuchsia::web::ContentDirectoryProvider> providers);

  // Serves `cdm_data_directory` to the instance as the `cdm_data` read-write
  // directory.
  void ServeCdmDataDirectory(
      fidl::InterfaceHandle<fuchsia::io::Directory> cdm_data_directory);

  // Serves `tmp_dir` to the instance as the `tmp` read-write directory.
  void ServeTmpDirectory(fuchsia::io::DirectoryHandle tmp_dir);

  // Sets an optional request to connect to the instance's `fuchsia.web/Debug`
  // protocol upon `Build()`.
  void SetDebugRequest(
      fidl::InterfaceRequest<fuchsia::web::Debug> debug_request);

  // Builds and returns the instance, or an error status value.
  Instance Build(
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request);

 private:
  InstanceBuilder(fuchsia::component::Realm& realm,
                  base::GUID id,
                  std::string name,
                  vfs::PseudoDir* instance_dir,
                  const base::CommandLine& launch_args);

  // Serves the arguments from the builder's `args()` command line in a file
  // named `argv.json` via the instance's `command-line-config` read-only
  // directory.
  void ServeCommandLine();

  // Serves `directory` as `name` in the instance's subtree as a read-only or
  // a read-write (if `writeable`) directory. `name` is both the name of the
  // directory and the name of the capability expected by the instance.
  void ServeDirectory(base::StringPiece name,
                      std::unique_ptr<vfs::internal::Directory> directory,
                      bool writeable);

  const raw_ref<fuchsia::component::Realm> realm_;
  const base::GUID id_;
  const std::string name_;
  raw_ptr<vfs::PseudoDir> instance_dir_;
  base::CommandLine args_;
  std::vector<fuchsia::component::decl::Offer> dynamic_offers_;
  fidl::InterfaceRequest<fuchsia::web::Debug> debug_request_;
};

// static
base::expected<std::unique_ptr<InstanceBuilder>, zx_status_t>
InstanceBuilder::Create(fuchsia::component::Realm& realm,
                        const base::CommandLine& launch_args) {
  // Pick a unique identifier for the new instance.
  base::GUID instance_id = base::GUID::GenerateRandomV4();
  auto instance_name = InstanceNameFromId(instance_id);

  // Create a pseudo-directory to contain the various directory capabilities
  // routed to this instance; i.e., `cdm_data`, `command-line-config`,
  // `content-directories`, `data`, and `tmp`. The builder is responsible for
  // removing it in case of error until `Build()` succeeds, at which point it is
  // the caller's responsibility to remove it when the instance goes away.
  auto instance_dir = std::make_unique<vfs::PseudoDir>();
  auto* const instance_dir_ptr = instance_dir.get();
  if (zx_status_t status = GetWebInstancesCollectionDir()->AddEntry(
          instance_name, std::move(instance_dir));
      status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "AddEntry(name)";
    return base::unexpected(status);
  }

  return base::ok(base::WrapUnique(new InstanceBuilder(
      realm, std::move(instance_id), std::move(instance_name), instance_dir_ptr,
      launch_args)));
}

InstanceBuilder::InstanceBuilder(fuchsia::component::Realm& realm,
                                 base::GUID id,
                                 std::string name,
                                 vfs::PseudoDir* instance_dir,
                                 const base::CommandLine& launch_args)
    : realm_(realm),
      id_(std::move(id)),
      name_(std::move(name)),
      instance_dir_(instance_dir),
      args_(launch_args) {}

InstanceBuilder::~InstanceBuilder() {
  if (instance_dir_) {
    DestroyInstanceDirectory(GetWebInstancesCollectionDir(), name_);
  }
}

void InstanceBuilder::AppendOffersForServices(
    const std::vector<std::string>& services) {
  for (const auto& service_name : services) {
    dynamic_offers_.push_back(fcdecl::Offer::WithProtocol(std::move(
        fcdecl::OfferProtocol()
            .set_source(fcdecl::Ref::WithParent({}))
            .set_source_name(service_name)
            .set_target_name(service_name)
            .set_dependency_type(fcdecl::DependencyType::STRONG)
            .set_availability(fcdecl::Availability::SAME_AS_TARGET))));
  }
}

void InstanceBuilder::ServeDataDirectory(
    fidl::InterfaceHandle<fuchsia::io::Directory> data_directory) {
  DCHECK(instance_dir_);
  ServeDirectory("data",
                 std::make_unique<vfs::RemoteDir>(std::move(data_directory)),
                 /*writeable=*/true);
}

zx_status_t InstanceBuilder::ServeContentDirectories(
    std::vector<fuchsia::web::ContentDirectoryProvider> providers) {
  DCHECK(instance_dir_);

  auto content_dirs = std::make_unique<vfs::PseudoDir>();

  for (auto& provider : providers) {
    zx_status_t status = content_dirs->AddEntry(
        provider.name(), std::make_unique<vfs::RemoteDir>(
                             std::move(*provider.mutable_directory())));
    if (status != ZX_OK) {
      ZX_LOG(ERROR, status)
          << "Conflicting content directory name \"" << provider.name() << "\"";
      return status;
    }
  }

  ServeDirectory("content-directories", std::move(content_dirs),
                 /*writeable=*/false);
  return ZX_OK;
}

void InstanceBuilder::ServeCdmDataDirectory(
    fidl::InterfaceHandle<fuchsia::io::Directory> cdm_data_directory) {
  DCHECK(instance_dir_);
  ServeDirectory(
      "cdm_data",
      std::make_unique<vfs::RemoteDir>(std::move(cdm_data_directory)),
      /*writeable=*/true);
}

void InstanceBuilder::ServeTmpDirectory(fuchsia::io::DirectoryHandle tmp_dir) {
  ServeDirectory("tmp", std::make_unique<vfs::RemoteDir>(std::move(tmp_dir)),
                 /*writeable=*/true);
}

void InstanceBuilder::SetDebugRequest(
    fidl::InterfaceRequest<fuchsia::web::Debug> debug_request) {
  debug_request_ = std::move(debug_request);
}

Instance InstanceBuilder::Build(
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request) {
  ServeCommandLine();

  fcdecl::Child child_decl;
  child_decl.set_name(name_);
  // TODO(crbug.com/1010222): Make kWebInstanceComponentUrl a relative
  // component URL and remove this workaround.
  // TODO(crbug.com/1395054): Better yet, replace the with_webui component with
  // direct routing of the resources from web_engine_shell.
  child_decl.set_url(
      base::CommandLine::ForCurrentProcess()->HasSwitch("with-webui")
          ? kWebInstanceWithWebUiComponentUrl
          : kWebInstanceComponentUrl);
  child_decl.set_startup(fcdecl::StartupMode::LAZY);

  ::fuchsia::component::CreateChildArgs create_child_args;
  create_child_args.set_dynamic_offers(std::move(dynamic_offers_));

  realm_->CreateChild(
      fcdecl::CollectionRef{.name = kCollectionName}, std::move(child_decl),
      std::move(create_child_args),
      [](::fuchsia::component::Realm_CreateChild_Result create_result) {
        LOG_IF(ERROR, create_result.is_err())
            << "CreateChild error: "
            << static_cast<uint32_t>(create_result.err());
      });

  fidl::InterfaceHandle<fuchsia::io::Directory> instance_services_handle;
  realm_->OpenExposedDir(
      fcdecl::ChildRef{.name = name_, .collection = kCollectionName},
      instance_services_handle.NewRequest(),
      [](::fuchsia::component::Realm_OpenExposedDir_Result open_result) {
        LOG_IF(ERROR, open_result.is_err())
            << "OpenExposedDir error: "
            << static_cast<uint32_t>(open_result.err());
      });

  sys::ServiceDirectory instance_services(std::move(instance_services_handle));
  fuchsia::component::BinderPtr binder_ptr;
  instance_services.Connect(binder_ptr.NewRequest());
  if (debug_request_) {
    instance_services.Connect(std::move(debug_request_));
  }
  instance_services.CloneChannel(std::move(services_request));

  // Ownership of the child and `instance_dir_` are now passed to the caller.
  instance_dir_ = nullptr;
  return Instance(id_, std::move(binder_ptr));
}

void InstanceBuilder::ServeCommandLine() {
  DCHECK(instance_dir_);

  if (args_.argv().size() < 2) {
    return;
  }

  auto config_dir = std::make_unique<vfs::PseudoDir>();

  std::vector<uint8_t> data(
      fuchsia_component_support::SerializeArguments(args_));
  const auto data_size = data.size();
  zx_status_t status = config_dir->AddEntry(
      "argv.json",
      std::make_unique<vfs::PseudoFile>(
          data_size, [data = std::move(data)](std::vector<uint8_t>* output,
                                              size_t max_bytes) {
            DCHECK_EQ(max_bytes, data.size());
            *output = data;
            return ZX_OK;
          }));
  ZX_DCHECK(status == ZX_OK, status);

  ServeDirectory("command-line-config", std::move(config_dir),
                 /*writeable=*/false);
}

void InstanceBuilder::ServeDirectory(
    base::StringPiece name,
    std::unique_ptr<vfs::internal::Directory> directory,
    bool writeable) {
  DCHECK(instance_dir_);
  zx_status_t status =
      instance_dir_->AddEntry(std::string(name), std::move(directory));
  ZX_DCHECK(status == ZX_OK, status);

  dynamic_offers_.push_back(fcdecl::Offer::WithDirectory(
      std::move(fcdecl::OfferDirectory()
                    .set_source(fcdecl::Ref::WithSelf({}))
                    .set_source_name(kCollectionName)
                    .set_target_name(std::string(name))
                    .set_rights(writeable ? ::fuchsia::io::RW_STAR_DIR
                                          : ::fuchsia::io::R_STAR_DIR)
                    .set_subdir(base::StrCat({name_, "/", name}))
                    .set_dependency_type(fcdecl::DependencyType::STRONG)
                    .set_availability(fcdecl::Availability::REQUIRED))));
}

void HandleCdmDataDirectoryParam(InstanceBuilder& builder,
                                 fuchsia::web::CreateContextParams& params) {
  if (!params.has_cdm_data_directory()) {
    return;
  }

  static constexpr char kCdmDataPath[] = "/cdm_data";

  builder.args().AppendSwitchNative(switches::kCdmDataDirectory, kCdmDataPath);
  builder.ServeCdmDataDirectory(
      std::move(*params.mutable_cdm_data_directory()));
  if (params.has_cdm_data_quota_bytes()) {
    builder.args().AppendSwitchNative(
        switches::kCdmDataQuotaBytes,
        base::NumberToString(params.cdm_data_quota_bytes()));
  }
}

void HandleDataDirectoryParam(InstanceBuilder& builder,
                              fuchsia::web::CreateContextParams& params) {
  if (!params.has_data_directory()) {
    // Caller requested a web instance without any peristence.
    builder.args().AppendSwitch(switches::kIncognito);
    return;
  }

  builder.ServeDataDirectory(std::move(*params.mutable_data_directory()));

  if (params.has_data_quota_bytes()) {
    builder.args().AppendSwitchNative(
        switches::kDataQuotaBytes,
        base::NumberToString(params.data_quota_bytes()));
  }
}

bool HandleContentDirectoriesParam(InstanceBuilder& builder,
                                   fuchsia::web::CreateContextParams& params) {
  if (!params.has_content_directories()) {
    return true;
  }

  for (const auto& directory : params.content_directories()) {
    if (!IsValidContentDirectoryName(directory.name())) {
      DLOG(ERROR) << "Invalid directory name: " << directory.name();
      return false;
    }
  }

  builder.args().AppendSwitch(switches::kEnableContentDirectories);
  return builder.ServeContentDirectories(
             std::move(*params.mutable_content_directories())) == ZX_OK;
}

}  // namespace

WebInstanceHost::WebInstanceHost() {
  // Ensure WebInstance is registered before launching it.
  // TODO(crbug.com/1211174): Replace with a different mechanism when available.
  RegisterWebInstanceProductData(kWebInstanceComponentUrl);
}

WebInstanceHost::~WebInstanceHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Uninitialize();
}

zx_status_t WebInstanceHost::CreateInstanceForContextWithCopiedArgs(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
    base::CommandLine extra_args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!is_initialized()) {
    Initialize();
  }

  auto expected_builder =
      InstanceBuilder::Create(*realm_, std::move(extra_args));
  if (!expected_builder.has_value()) {
    return expected_builder.error();
  }
  auto& builder = expected_builder.value();

  if (zx_status_t status = AppendLaunchArgs(params, builder->args());
      status != ZX_OK) {
    return status;
  }

  // TODO(grt): What to do about `params.service_directory`? At the moment, we
  // require that all of web_instance's required and optional protocols are
  // routed from the embedding component's parent.

  {
    std::vector<std::string> services;
    AppendDynamicServices(params.features(), params.has_playready_key_system(),
                          services);
    builder->AppendOffersForServices(services);
  }

  HandleCdmDataDirectoryParam(*builder, params);

  HandleDataDirectoryParam(*builder, params);

  if (!HandleContentDirectoriesParam(*builder, params)) {
    return ZX_ERR_INVALID_ARGS;
  }

  // TODO(crbug.com/1395774): Replace this with normal routing of tmp from
  // web_engine_shell's parent down to web_instance.
  if (tmp_dir_.is_valid()) {
    builder->ServeTmpDirectory(std::move(tmp_dir_));
  }

  // If one or more Debug protocol clients are active then enable debugging,
  // and connect the instance to the fuchsia.web.Debug proxy.
  if (debug_proxy_.has_clients()) {
    builder->args().AppendSwitch(switches::kEnableRemoteDebugMode);
    fidl::InterfaceHandle<fuchsia::web::Debug> debug_handle;
    builder->SetDebugRequest(debug_handle.NewRequest());
    debug_proxy_.RegisterInstance(std::move(debug_handle));
  }

  auto instance = builder->Build(std::move(services_request));
  // Monitor the instance's Binder to track its destruction.
  instance.binder_ptr.set_error_handler(
      [this, id = instance.id](zx_status_t status) {
        this->OnComponentBinderClosed(id, status);
      });
  instances_.emplace(std::move(instance.id), std::move(instance.binder_ptr));

  return ZX_OK;
}

void WebInstanceHost::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!realm_);

  zx_status_t status =
      base::ComponentContextForProcess()->svc()->Connect(realm_.NewRequest());
  ZX_CHECK(status == ZX_OK, status) << "Connect(fuchsia.component/Realm)";
  realm_.set_error_handler(
      fit::bind_member(this, &WebInstanceHost::OnRealmError));
}

void WebInstanceHost::Uninitialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Destroy all child instances and each one's outgoing directory subtree.
  auto* const instances_dir = GetWebInstancesCollectionDir();
  for (auto& [id, binder_ptr] : instances_) {
    const std::string name(InstanceNameFromId(id));
    if (realm_) {
      DestroyInstance(*realm_, name);
    }
    DestroyInstanceDirectory(instances_dir, name);
    binder_ptr.Unbind();
  }
  instances_.clear();

  realm_.Unbind();

  // Note: the entry in the outgoing directory for the top-level instances dir
  // is leaked in support of having multiple hosts active in a single process.
}

void WebInstanceHost::OnRealmError(zx_status_t status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ZX_LOG(ERROR, status) << "RealmBuilder channel error";
  Uninitialize();
}

void WebInstanceHost::OnComponentBinderClosed(const base::GUID& id,
                                              zx_status_t status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Destroy the child instance.
  const std::string name(InstanceNameFromId(id));
  DestroyInstance(*realm_, name);

  // Drop the directory subtree for the child instance.
  DestroyInstanceDirectory(GetWebInstancesCollectionDir(), name);

  // Drop the hold on the instance's Binder. Note: destroying the InterfacePtr
  // here also deletes the lambda into which `id` was bound, so `id` must not
  // be touched after this next statement.
  auto count = instances_.erase(id);
  DCHECK_EQ(count, 1UL);

  if (instances_.empty()) {
    Uninitialize();
  }
}
