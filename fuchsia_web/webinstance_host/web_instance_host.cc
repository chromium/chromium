// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webinstance_host/web_instance_host.h"

#include <fuchsia/component/decl/cpp/fidl.h>
#include <fuchsia/io/cpp/fidl.h>
#include <lib/fit/function.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <lib/vfs/cpp/pseudo_dir.h>
#include <lib/vfs/cpp/pseudo_file.h>
#include <lib/vfs/cpp/remote_dir.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/fixed_flat_map.h"
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
#include "fuchsia_web/webinstance_host/web_instance_host_constants.h"
#include "fuchsia_web/webinstance_host/web_instance_host_internal.h"

namespace {

namespace fcdecl = ::fuchsia::component::decl;

// The name of the component collection hosting the instances.
constexpr char kCollectionName[] = "web_instances";

// Returns the URL of the WebInstance component to be launched.
std::string MakeWebInstanceComponentUrl(bool with_webui,
                                        bool with_service_directory) {
  // TODO(crbug.com/1010222): Use a relative component URL when the hosting
  // component is in the same package as web_instance.cm and remove this
  // workaround.
  return base::StrCat(
      {"fuchsia-pkg://fuchsia.com/",
       (with_webui ? "web_engine_with_webui" : "web_engine"), "#meta/",
       (with_service_directory ? "web_instance_with_svc_directory.cm"
                               : "web_instance.cm")});
}

// Returns the "/web_instances" dir from the component's outgoing directory,
// creating it if necessary.
vfs::PseudoDir* GetWebInstancesCollectionDir(
    sys::OutgoingDirectory& outgoing_directory) {
  return outgoing_directory.GetOrCreateDirectory(kCollectionName);
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
      sys::OutgoingDirectory& outgoing_directory,
      fuchsia::component::Realm& realm,
      const base::CommandLine& launch_args);
  ~InstanceBuilder();

  base::CommandLine& args() { return args_; }

  // Offers the services named in `services` to the instance as dynamic
  // protocol offers.
  void AppendOffersForServices(const std::vector<std::string>& services);

  // Serves `service_directory` to the instance as the 'svc' read-write
  // directory.
  void ServeServiceDirectory(
      fidl::InterfaceHandle<fuchsia::io::Directory> service_directory);

  // Offers the read-only root-ssl-certificates directory from the parent.
  void ServeRootSslCertificates();

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
      const std::string& instance_component_url,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request);

 private:
  InstanceBuilder(sys::OutgoingDirectory& outgoing_directory,
                  fuchsia::component::Realm& realm,
                  base::GUID id,
                  std::string name,
                  vfs::PseudoDir* instance_dir,
                  const base::CommandLine& launch_args);

  // Serves the arguments from the builder's `args()` command line in a file
  // named `argv.json` via the instance's `command-line-config` read-only
  // directory.
  void ServeCommandLine();

  // Adds offers from `void` for any optionally-offered directories that are not
  // being served for the invoker.
  void OfferMissingDirectoriesFromVoid();

  // The directories that are optionally offered to `web_instance.cm` based on
  // the invoker's configuration.
  enum class OptionalDirectory {
    kFirst = 0,
    kCdmData = kFirst,
    kCommandLineConfig,
    kContentDirectories,
    kData,
    kTmp,
    kCount,
  };

  // Returns a bitmask for `directory` for use with the `served_directories_`
  // bitfield.
  static uint32_t directory_bitmask(OptionalDirectory directory) {
    return 1u << static_cast<int>(directory);
  }

  // Returns true if the host will serve `directory` to the instance.
  bool is_directory_served(OptionalDirectory directory) const {
    return served_directories_ & directory_bitmask(directory);
  }

  // Records that `directory` will be served to the instance.
  void set_directory_served(OptionalDirectory directory) {
    served_directories_ |= directory_bitmask(directory);
  }

  // Returns the capability and directory name for `directory`.
  static base::StringPiece GetDirectoryName(OptionalDirectory directory);

  // Serves `fs_directory` as `directory`. `fs_directory` may be specific to
  // this instance (e.g., persistent data storage) or required only in
  // particular configurations (e.g., CDM data storage), to the instance. Most
  // common read-only directories (e.g., "root-ssl-certificates") should instead
  // be offered statically to the `web_instances` collection.
  void ServeOptionalDirectory(
      OptionalDirectory directory,
      std::unique_ptr<vfs::internal::Directory> fs_directory,
      fuchsia::io::Operations rights);

  // Offers the directory `directory` from `void`.
  void OfferOptionalDirectoryFromVoid(OptionalDirectory directory);

  // Serves the directory `name` as `offer` in the instance's subtree as a
  // read-only or a read-write (if `writeable`) directory.
  void ServeDirectory(base::StringPiece name,
                      std::unique_ptr<vfs::internal::Directory> fs_directory,
                      fuchsia::io::Operations rights);

  const raw_ref<sys::OutgoingDirectory> outgoing_directory_;
  const raw_ref<fuchsia::component::Realm> realm_;
  const base::GUID id_;
  const std::string name_;
  raw_ptr<vfs::PseudoDir> instance_dir_;
  base::CommandLine args_;

  // A bitfield of `directory_bitmask()` values indicating which optional
  // directories are being served to the instance.
  uint32_t served_directories_ = 0;
  std::vector<fuchsia::component::decl::Offer> dynamic_offers_;
  fidl::InterfaceRequest<fuchsia::web::Debug> debug_request_;
};

// static
base::expected<std::unique_ptr<InstanceBuilder>, zx_status_t>
InstanceBuilder::Create(sys::OutgoingDirectory& outgoing_directory,
                        fuchsia::component::Realm& realm,
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
  if (zx_status_t status =
          GetWebInstancesCollectionDir(outgoing_directory)
              ->AddEntry(instance_name, std::move(instance_dir));
      status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "AddEntry(name)";
    return base::unexpected(status);
  }

  return base::WrapUnique(new InstanceBuilder(
      outgoing_directory, realm, std::move(instance_id),
      std::move(instance_name), instance_dir_ptr, launch_args));
}

InstanceBuilder::InstanceBuilder(sys::OutgoingDirectory& outgoing_directory,
                                 fuchsia::component::Realm& realm,
                                 base::GUID id,
                                 std::string name,
                                 vfs::PseudoDir* instance_dir,
                                 const base::CommandLine& launch_args)
    : outgoing_directory_(outgoing_directory),
      realm_(realm),
      id_(std::move(id)),
      name_(std::move(name)),
      instance_dir_(instance_dir),
      args_(launch_args) {}

InstanceBuilder::~InstanceBuilder() {
  if (instance_dir_) {
    DestroyInstanceDirectory(GetWebInstancesCollectionDir(*outgoing_directory_),
                             name_);
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

void InstanceBuilder::ServeServiceDirectory(
    fidl::InterfaceHandle<fuchsia::io::Directory> service_directory) {
  DCHECK(instance_dir_);
  ServeDirectory(
      "svc", std::make_unique<vfs::RemoteDir>(std::move(service_directory)),
      fuchsia::io::Operations::CONNECT | fuchsia::io::Operations::ENUMERATE |
          fuchsia::io::Operations::TRAVERSE);
}

void InstanceBuilder::ServeDataDirectory(
    fidl::InterfaceHandle<fuchsia::io::Directory> data_directory) {
  DCHECK(instance_dir_);
  ServeOptionalDirectory(
      OptionalDirectory::kData,
      std::make_unique<vfs::RemoteDir>(std::move(data_directory)),
      fuchsia::io::RW_STAR_DIR);
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

  ServeOptionalDirectory(OptionalDirectory::kContentDirectories,
                         std::move(content_dirs), fuchsia::io::R_STAR_DIR);
  return ZX_OK;
}

void InstanceBuilder::ServeCdmDataDirectory(
    fidl::InterfaceHandle<fuchsia::io::Directory> cdm_data_directory) {
  DCHECK(instance_dir_);
  ServeOptionalDirectory(
      OptionalDirectory::kCdmData,
      std::make_unique<vfs::RemoteDir>(std::move(cdm_data_directory)),
      fuchsia::io::RW_STAR_DIR);
}

void InstanceBuilder::ServeTmpDirectory(fuchsia::io::DirectoryHandle tmp_dir) {
  ServeOptionalDirectory(OptionalDirectory::kTmp,
                         std::make_unique<vfs::RemoteDir>(std::move(tmp_dir)),
                         fuchsia::io::RW_STAR_DIR);
}

void InstanceBuilder::SetDebugRequest(
    fidl::InterfaceRequest<fuchsia::web::Debug> debug_request) {
  debug_request_ = std::move(debug_request);
}

Instance InstanceBuilder::Build(
    const std::string& instance_component_url,
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request) {
  ServeCommandLine();

  // Create dynamic offers from `void` for any optional directories
  // expected by web_instance.cm that are not being provided by the invoker.
  OfferMissingDirectoriesFromVoid();

  fcdecl::Child child_decl;
  child_decl.set_name(name_);
  child_decl.set_url(instance_component_url);
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

  ServeOptionalDirectory(OptionalDirectory::kCommandLineConfig,
                         std::move(config_dir), fuchsia::io::R_STAR_DIR);
}

void InstanceBuilder::OfferMissingDirectoriesFromVoid() {
  for (auto directory = OptionalDirectory::kFirst;
       directory != OptionalDirectory::kCount;
       directory =
           static_cast<OptionalDirectory>(static_cast<int>(directory) + 1)) {
    if (!is_directory_served(directory)) {
      OfferOptionalDirectoryFromVoid(directory);
    }
  }
}

// static
base::StringPiece InstanceBuilder::GetDirectoryName(
    OptionalDirectory directory) {
  static constexpr auto kNames =
      base::MakeFixedFlatMap<OptionalDirectory, base::StringPiece>({
          {OptionalDirectory::kCdmData, "cdm_data"},
          {OptionalDirectory::kCommandLineConfig, "command-line-config"},
          {OptionalDirectory::kContentDirectories, "content-directories"},
          {OptionalDirectory::kData, "data"},
          {OptionalDirectory::kTmp, "tmp"},
      });
  static_assert(kNames.size() == static_cast<int>(OptionalDirectory::kCount));
  return kNames.at(directory);
}

void InstanceBuilder::ServeOptionalDirectory(
    OptionalDirectory directory,
    std::unique_ptr<vfs::internal::Directory> fs_directory,
    fuchsia::io::Operations rights) {
  DCHECK(instance_dir_);
  DCHECK(!is_directory_served(directory));

  set_directory_served(directory);
  ServeDirectory(GetDirectoryName(directory), std::move(fs_directory), rights);
}

void InstanceBuilder::OfferOptionalDirectoryFromVoid(
    OptionalDirectory directory) {
  DCHECK(!is_directory_served(directory));

  const auto name = GetDirectoryName(directory);
  dynamic_offers_.push_back(fcdecl::Offer::WithDirectory(
      std::move(fcdecl::OfferDirectory()
                    .set_source(fcdecl::Ref::WithVoidType({}))
                    .set_source_name(std::string(name))
                    .set_target_name(std::string(name))
                    .set_dependency_type(fcdecl::DependencyType::STRONG)
                    .set_availability(fcdecl::Availability::OPTIONAL))));
}

void InstanceBuilder::ServeDirectory(
    base::StringPiece name,
    std::unique_ptr<vfs::internal::Directory> fs_directory,
    fuchsia::io::Operations rights) {
  DCHECK(instance_dir_);
  zx_status_t status =
      instance_dir_->AddEntry(std::string(name), std::move(fs_directory));
  ZX_DCHECK(status == ZX_OK, status);

  dynamic_offers_.push_back(fcdecl::Offer::WithDirectory(
      std::move(fcdecl::OfferDirectory()
                    .set_source(fcdecl::Ref::WithSelf({}))
                    .set_source_name(kCollectionName)
                    .set_target_name(std::string(name))
                    .set_rights(rights)
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

WebInstanceHost::WebInstanceHost(sys::OutgoingDirectory& outgoing_directory)
    : outgoing_directory_(outgoing_directory) {}

WebInstanceHost::~WebInstanceHost() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  Uninitialize();
}

zx_status_t WebInstanceHost::CreateInstanceForContextWithCopiedArgs(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
    base::CommandLine extra_args) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const bool with_service_directory = params.has_service_directory();

  // True if the process includes `--with-webui` on its command line. This is a
  // test-only feature for `web_engine_shell` that causes `web_instance.cm` to
  // be run from the `web_engine_with_webui` package rather than the production
  // `web_engine` package.
  const bool with_webui =
      base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kWithWebui);

  // Web UI resources are not supported with a service directory.
  if (with_webui && with_service_directory) {
    return ZX_ERR_INVALID_ARGS;
  }

  if (!is_initialized()) {
    Initialize();
  }

  auto expected_builder = InstanceBuilder::Create(*outgoing_directory_, *realm_,
                                                  std::move(extra_args));
  if (!expected_builder.has_value()) {
    return expected_builder.error();
  }
  auto& builder = expected_builder.value();

  if (zx_status_t status = AppendLaunchArgs(params, builder->args());
      status != ZX_OK) {
    return status;
  }

  if (with_service_directory) {
    builder->ServeServiceDirectory(
        std::move(*params.mutable_service_directory()));
  } else {
    std::vector<std::string> services;
    const auto features = params.has_features()
                              ? params.features()
                              : fuchsia::web::ContextFeatureFlags();
    AppendDynamicServices(features, params.has_playready_key_system(),
                          services);
    builder->AppendOffersForServices(services);
  }

  // The `config-data` directory is statically offered to all instances.
  // The `root-ssl-certificates` directory is statically offered to all
  // instances regardless of whether networking is requested.

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

  const auto instance_component_url =
      MakeWebInstanceComponentUrl(with_webui, with_service_directory);

  // Ensure WebInstance is registered before launching it.
  // TODO(crbug.com/1211174): Replace with a different mechanism when available.
  RegisterWebInstanceProductData(instance_component_url);

  // TODO(crbug.com/1395054): Replace the with_webui component with direct
  // routing of the resources from web_engine_shell.
  auto instance =
      builder->Build(instance_component_url, std::move(services_request));
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
  auto* const instances_dir =
      GetWebInstancesCollectionDir(*outgoing_directory_);
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
  DestroyInstanceDirectory(GetWebInstancesCollectionDir(*outgoing_directory_),
                           name);

  // Drop the hold on the instance's Binder. Note: destroying the InterfacePtr
  // here also deletes the lambda into which `id` was bound, so `id` must not
  // be touched after this next statement.
  auto count = instances_.erase(id);
  DCHECK_EQ(count, 1UL);

  if (instances_.empty()) {
    Uninitialize();
  }
}
