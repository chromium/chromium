// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_CAST_PENDING_CAST_COMPONENT_H_
#define FUCHSIA_WEB_RUNNERS_CAST_PENDING_CAST_COMPONENT_H_

#include <fidl/chromium.cast/cpp/fidl.h>
#include <fuchsia/component/runner/cpp/fidl.h>
#include <lib/fidl/cpp/interface_request.h>

#include <memory>
#include <string_view>

#include "base/fuchsia/fidl_event_handler.h"
#include "fuchsia_web/runners/cast/cast_component.h"

namespace base {
class StartupContext;
}  // namespace base

// Manages asynchronous retrieval of parameters required to launch the specified
// Cast |app_id|.
class PendingCastComponent {
 public:
  // Implemented by the PendingCastComponent's owner, to actually launch the
  // component, or cancel it.
  class Delegate {
   public:
    virtual ~Delegate() = default;

    // Called to launch the component with |params|. The Delegate should
    // clean up resources associated with |component| and delete it.
    virtual void LaunchPendingComponent(PendingCastComponent* component,
                                        CastComponent::Params params) = 0;

    // Called if an error occurs, to have the Delegate clean up resources
    // associated with |component|, and delete it.
    virtual void CancelPendingComponent(PendingCastComponent* component) = 0;
  };

  PendingCastComponent(
      Delegate* delegate,
      std::unique_ptr<base::StartupContext> startup_context,
      fidl::InterfaceRequest<fuchsia::component::runner::ComponentController>
          controller_request,
      std::string_view app_id);
  ~PendingCastComponent();

  PendingCastComponent(const PendingCastComponent&) = delete;
  PendingCastComponent& operator=(const PendingCastComponent&) = delete;

  const std::string_view app_id() const { return app_id_; }

 private:
  void RequestCorsExemptHeaders();

  // Handlers for completing initialization of some |params_| fields.
  void OnApplicationConfigReceived(
      chromium::cast::ApplicationConfig app_config);
  void OnApiBindingsInitialized();

  // Passes |params_| to |delegate_| if they are complete, to use to launch the
  // component. |this| will be deleted before the call returns in that case.
  // Has no effect if |params_| are not yet complete.
  void MaybeLaunchComponent();

  void OnApplicationContextFidlError(fidl::UnbindInfo error);

  void CancelComponent();

  // Reference to the Delegate which manages |this|.
  Delegate* const delegate_;

  // Id of the Cast application that this instance describes.
  const std::string app_id_;

  // Parameters required to construct the CastComponent.
  CastComponent::Params params_;

  // Used to receive the media session Id and ApplicationConfig.
  fidl::Client<chromium_cast::ApplicationContext> application_context_;
  base::FidlErrorEventHandler<chromium_cast::ApplicationContext>
      application_context_error_handler_;
  chromium::cast::ApplicationConfigManagerPtr application_config_manager_;
};

#endif  // FUCHSIA_WEB_RUNNERS_CAST_PENDING_CAST_COMPONENT_H_
