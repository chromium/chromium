// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/runners/cast/cast_runner_v1.h"

#include <fuchsia/component/cpp/fidl.h>
#include <fuchsia/component/decl/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/ui/app/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/interface_request.h>
#include <lib/sys/cpp/outgoing_directory.h>
#include <lib/sys/cpp/service_directory.h>
#include <memory>
#include <string>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/startup_context.h"
#include "base/guid.h"
#include "base/logging.h"
#include "fuchsia_web/runners/cast/fidl/fidl/chromium/cast/cpp/fidl.h"
#include "fuchsia_web/runners/common/modular/agent_manager.h"
#include "url/gurl.h"

namespace {

constexpr char kCollection[] = "v1-activities";

vfs::PseudoDir* SvcForCfv2Dir() {
  constexpr char kSvcForCfv2Path[] = "svc_for_cfv2";
  return base::ComponentContextForProcess()->outgoing()->GetOrCreateDirectory(
      kSvcForCfv2Path);
}

// Retains the state necessary to managed a Cast CFv2 activity, running content
// on behalf of a Cast activity launched via CFv1.
// This class self-deletes when its work is complete, or when requested.
class CastComponentV1 : public fuchsia::sys::ComponentController {
 public:
  CastComponentV1(GURL component_url,
                  std::unique_ptr<base::StartupContext> startup_context,
                  fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                      controller_request,
                  std::string agent_url)
      : component_url_(std::move(component_url)),
        startup_context_(std::move(startup_context)),
        agent_url_(std::move(agent_url)),
        child_id_(base::GUID::GenerateRandomV4().AsLowercaseString()),
        agent_manager_(startup_context_->svc()) {
    // Bind the ComponentController request, if provided, to trigger teardown.
    if (controller_request.is_valid()) {
      controller_binding_.Bind(std::move(controller_request));
      controller_binding_.set_error_handler([this](zx_status_t status) {
        ZX_LOG_IF(ERROR, status != ZX_ERR_PEER_CLOSED, status)
            << "ComponentController disconnected for " << component_url_;
        delete this;
      });
    }

    // Create the service-directory to offer to the CFv2 component.
    auto child_svc = std::make_unique<vfs::PseudoDir>();
    svc_for_cfv2_ = child_svc.get();
    zx_status_t status =
        SvcForCfv2Dir()->AddEntry(child_id_, std::move(child_svc));
    ZX_CHECK(status == ZX_OK, status);

    // TODO(crbug.com/1332972): Migrate the CFv2 code not to need these routed
    // via the Cast activity's incoming services.
    OfferFromStartupContext<chromium::cast::ApplicationConfigManager>();

    // Offer services from the associated Agent to the CFv2 component.
    OfferFromAgent<chromium::cast::ApiBindings>();
    OfferFromAgent<chromium::cast::ApplicationContext>();
    OfferFromAgent<chromium::cast::UrlRequestRewriteRulesProvider>();

    // Expose services from the CFv2 component, via the CFv1 component's
    // outgoing directory.
    ExposeFromCfv2Component<fuchsia::ui::app::ViewProvider>();
    ExposeFromCfv2Component<fuchsia::modular::Lifecycle>();

    // TODO(crbug.com/1120914): Remove this with the FrameHost component.
    ExposeFromCfv2Component<fuchsia::web::FrameHost>();

    // Offer the sub-directory of the "cvs_for_cfv2" capability created above,
    // for use as the component's "/svc".
    fuchsia::component::CreateChildArgs args;
    args.mutable_dynamic_offers()->push_back(
        fuchsia::component::decl::Offer::WithDirectory(std::move(
            fuchsia::component::decl::OfferDirectory()
                .set_source(fuchsia::component::decl::Ref::WithSelf({}))
                .set_source_name("svc_for_cfv2")
                .set_subdir(child_id_)
                .set_target_name("svc")
                .set_rights(fuchsia::io::RW_STAR_DIR)
                .set_dependency_type(
                    fuchsia::component::decl::DependencyType::STRONG))));

    // Connect to the runner component's framework-provided Realm protocol.
    base::ComponentContextForProcess()->svc()->Connect(realm_.NewRequest());
    realm_.set_error_handler([this](zx_status_t status) {
      ZX_LOG(ERROR, status) << "Realm disconnected.";
      delete this;
    });

    // Start the Cast application as a CFv2 child component.
    fuchsia::component::decl::Child child;
    child.set_name(child_id_);
    child.set_url(component_url_.spec());
    child.set_startup(fuchsia::component::decl::StartupMode::LAZY);

    // Start the child and connect to the directory of capabilities it exposes.
    realm_->CreateChild(
        fuchsia::component::decl::CollectionRef{.name = kCollection},
        std::move(child), std::move(args),
        fit::bind_member(this, &CastComponentV1::OnCreateChildComplete));

    fidl::InterfaceHandle<fuchsia::io::Directory> exposed_dir;
    realm_->OpenExposedDir(
        fuchsia::component::decl::ChildRef{.name = child_id_,
                                           .collection = kCollection},
        exposed_dir.NewRequest(),
        fit::bind_member(this, &CastComponentV1::OnOpenExposedDirComplete));

    exposed_from_cfv2_ =
        std::make_unique<sys::ServiceDirectory>(std::move(exposed_dir));

    // Use Binder to trigger the component to start, and to detect if it
    // stops itself.
    binder_ = exposed_from_cfv2_->Connect<fuchsia::component::Binder>();
    binder_.set_error_handler([this](zx_status_t) {
      // Although the ComponentController will have reported a status to the
      // framework when closing, this is not reflected in the `Binder` status.
      // Deleting `this` will cause the stopped child component to actually
      // be removed from the collection.
      delete this;
    });

    // Start serving requests to the CFv1 outgoing directory.
    startup_context_->ServeOutgoingDirectory();
  }

 private:
  ~CastComponentV1() override {
    // Request asynchronous teardown of the child component.
    if (realm_) {
      realm_->DestroyChild(
          fuchsia::component::decl::ChildRef{.name = child_id_,
                                             .collection = kCollection},
          [](auto) {});
    }

    // Tear-down the service-directory entries provided to the child.
    if (svc_for_cfv2_) {
      zx_status_t status = SvcForCfv2Dir()->RemoveEntry(child_id_);
      ZX_CHECK(status == ZX_OK, status);
    }

    // Report the reason for termination, if possible.
    if (controller_binding_.is_bound()) {
      controller_binding_.events().OnTerminated(
          exit_code_, fuchsia::sys::TerminationReason::EXITED);
    }
  }

  // fuchsia::sys::ComponentController implementation.
  void Kill() override {
    // Termination in response to `Kill()` is always expected.
    exit_code_ = ZX_OK;

    // Teardown of the CFv2 component will be observed via `binder`.
    realm_->DestroyChild(
        fuchsia::component::decl::ChildRef{.name = child_id_,
                                           .collection = kCollection},
        [](auto) {});

    // Clear the `realm_`, since we already destroyed the child.
    realm_ = nullptr;
  }
  void Detach() override {
    // We don't support detaching Cast activities.
    exit_code_ = ZX_ERR_NOT_SUPPORTED;
    delete this;
  }

  template <typename Interface>
  void OfferFromStartupContext() {
    zx_status_t status = svc_for_cfv2_->AddEntry(
        Interface::Name_,
        std::make_unique<vfs::Service>(fidl::InterfaceRequestHandler<Interface>(
            [this](fidl::InterfaceRequest<Interface> request) {
              startup_context_->svc()->Connect(std::move(request));
            })));
    ZX_CHECK(status == ZX_OK, status);
  }

  template <typename Interface>
  void OfferFromAgent() {
    zx_status_t status = svc_for_cfv2_->AddEntry(
        Interface::Name_,
        std::make_unique<vfs::Service>(fidl::InterfaceRequestHandler<Interface>(
            [this](fidl::InterfaceRequest<Interface> request) {
              agent_manager_.ConnectToAgentService(agent_url_,
                                                   std::move(request));
            })));
    ZX_CHECK(status == ZX_OK, status);
  }

  template <typename Interface>
  void ExposeFromCfv2Component() {
    zx_status_t status = startup_context_->outgoing()->AddPublicService(
        fidl::InterfaceRequestHandler<Interface>(
            [this](fidl::InterfaceRequest<Interface> request) {
              exposed_from_cfv2_->Connect(std::move(request));
            }));
    ZX_CHECK(status == ZX_OK, status);
  }

  void OnCreateChildComplete(
      fuchsia::component::Realm_CreateChild_Result result) {
    if (result.is_err()) {
      LOG(ERROR) << "CreateChild failed: " << (int)result.err();
      delete this;
      return;
    }
  }

  void OnOpenExposedDirComplete(
      fuchsia::component::Realm_OpenExposedDir_Result result) {
    if (result.is_err()) {
      LOG(ERROR) << "OpenExposedDir failed: " << (int)result.err();
      delete this;
      return;
    }

    // Opening the directory exposed by the CFV2 component succeeded,
    // so we can assume that it started correctly.
    exit_code_ = ZX_OK;
  }

  const GURL component_url_;
  const std::unique_ptr<base::StartupContext> startup_context_;
  const std::string agent_url_;
  const std::string child_id_;

  // Binds the ComponentController request to this implementation.
  fidl::Binding<fuchsia::sys::ComponentController> controller_binding_{this};

  // Used to connect to services provided by the Agent that owns the activity.
  cr_fuchsia::AgentManager agent_manager_;

  // Holds the complete set of services to be offered to the CFv2 activity.
  vfs::PseudoDir* svc_for_cfv2_ = nullptr;

  // Holds a channel to the CFv2 component's outgoing directory.
  std::unique_ptr<sys::ServiceDirectory> exposed_from_cfv2_;

  // Used to manage ephemeral child components.
  fuchsia::component::RealmPtr realm_;

  // Used to observe if the CFv2 component stops itself.
  fuchsia::component::BinderPtr binder_;

  // Exit-code reported to the ComponentController, if bound, on exit.
  int64_t exit_code_ = ZX_ERR_INTERNAL;
};

// Maintains the state associated with a new Cast activity while the owning
// Agent URL is being resolved.
// This class self-deletes when its work is complete.
class PendingCastComponentV1 {
 public:
  PendingCastComponentV1(
      GURL component_url,
      std::unique_ptr<base::StartupContext> startup_context,
      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
          controller_request)
      : component_url_(std::move(component_url)),
        startup_context_(std::move(startup_context)),
        controller_request_(std::move(controller_request)) {
    // Request the application's configuration, including the identity of the
    // Agent that should provide component-specific resources, e.g. API
    // bindings.
    // TODO(https://crbug.com/1065707): Access the ApplicationConfigManager via
    // the Runner's incoming service directory once it is available there.
    startup_context_->svc()->Connect(application_config_manager_.NewRequest());
    application_config_manager_.set_error_handler([this](zx_status_t status) {
      ZX_LOG(ERROR, status) << "ApplicationConfigManager disconnected.";
      delete this;
    });
    application_config_manager_->GetConfig(
        component_url_.GetContent(),
        fit::bind_member(this,
                         &PendingCastComponentV1::OnApplicationConfigReceived));
  }

 private:
  void OnApplicationConfigReceived(
      chromium::cast::ApplicationConfig application_config) {
    if (application_config.has_agent_url()) {
      new CastComponentV1(std::move(component_url_),
                          std::move(startup_context_),
                          std::move(controller_request_),
                          std::move(*application_config.mutable_agent_url()));
    } else {
      LOG(ERROR) << "No Agent is associated with this application.";
    }
    delete this;
  }

  GURL component_url_;
  std::unique_ptr<base::StartupContext> startup_context_;
  fidl::InterfaceRequest<fuchsia::sys::ComponentController> controller_request_;

  // Used to obtain the component URL of the owning Agent.
  chromium::cast::ApplicationConfigManagerPtr application_config_manager_;
};

}  // namespace

CastRunnerV1::CastRunnerV1() {
  std::ignore = SvcForCfv2Dir();
}

CastRunnerV1::~CastRunnerV1() = default;

void CastRunnerV1::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  // Verify that |package| specifies a Cast URI, before servicing the request.
  constexpr char kCastPresentationUrlScheme[] = "cast";
  constexpr char kCastSecurePresentationUrlScheme[] = "casts";

  GURL cast_url(package.resolved_url);
  if (!cast_url.is_valid() ||
      (!cast_url.SchemeIs(kCastPresentationUrlScheme) &&
       !cast_url.SchemeIs(kCastSecurePresentationUrlScheme)) ||
      cast_url.GetContent().empty()) {
    LOG(ERROR) << "Rejected invalid URL: " << cast_url;
    return;
  }

  auto startup_context =
      std::make_unique<base::StartupContext>(std::move(startup_info));

  if (!startup_context->has_outgoing_directory_request()) {
    LOG(ERROR) << "Missing outgoing directory request";
    return;
  }

  // TODO(crbug.com/1120914): Remove this once Component Framework v2 can be
  // used to route fuchsia.web.FrameHost capabilities cleanly.
  static constexpr char kFrameHostComponentName[] =
      "cast:fuchsia.web.FrameHost";
  if (cast_url.spec() == kFrameHostComponentName) {
    new CastComponentV1(std::move(cast_url), std::move(startup_context),
                        std::move(controller_request), std::string());
    return;
  }

  new PendingCastComponentV1(std::move(cast_url), std::move(startup_context),
                             std::move(controller_request));
}
