// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_WEB_COMPONENT_H_
#define FUCHSIA_RUNNERS_COMMON_WEB_COMPONENT_H_

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/ui/app/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <lib/fidl/cpp/binding_set.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/service_directory_client.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "fuchsia/base/lifecycle_impl.h"
#include "url/gurl.h"

class WebContentRunner;

// Base component implementation for web-based content Runners. Each instance
// manages the lifetime of its own fuchsia::web::Frame, including associated
// resources and service bindings.  Runners for specialized web-based content
// (e.g. Cast applications) can extend this class to configure the Frame to
// their needs, publish additional APIs, etc.
class WebComponent : public fuchsia::sys::ComponentController,
                     public fuchsia::ui::app::ViewProvider {
 public:
  // Creates a WebComponent encapsulating a web.Frame. A ViewProvider service
  // will be published to the service-directory specified by |startup_context|,
  // and if |controller_request| is valid then it will be bound to this
  // component, and the component configured to teardown if that channel closes.
  // |runner| must outlive this component.
  WebComponent(WebContentRunner* runner,
               std::unique_ptr<base::fuchsia::StartupContext> context,
               fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                   controller_request);

  ~WebComponent() override;

  // Enables remote debugging on this WebComponent. Must be called before
  // StartComponent().
  void EnableRemoteDebugging();

  // Starts this component. Must be called before LoadUrl().
  virtual void StartComponent();

  // Navigates this component's Frame to |url| and passes |extra_headers|.
  // May not be called until after StartComponent().
  void LoadUrl(const GURL& url,
               std::vector<fuchsia::net::http::Header> extra_headers);

  fuchsia::web::Frame* frame() const { return frame_.get(); }

 protected:
  // fuchsia::sys::ComponentController implementation.
  void Kill() override;
  void Detach() override;

  // fuchsia::ui::app::ViewProvider implementation.
  void CreateView(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override;

  // Reports the supplied exit-code and reason to the |controller_binding_| and
  // requests that the |runner_| delete this component.
  virtual void DestroyComponent(int termination_exit_code,
                                fuchsia::sys::TerminationReason reason);

  // Returns the component's startup context (e.g. incoming services, public
  // service directory, etc).
  base::fuchsia::StartupContext* startup_context() const {
    return startup_context_.get();
  }

 private:
  WebContentRunner* const runner_ = nullptr;
  const std::unique_ptr<base::fuchsia::StartupContext> startup_context_;

  fuchsia::web::FramePtr frame_;

  // Bindings used to manage the lifetime of this component instance.
  fidl::Binding<fuchsia::sys::ComponentController> controller_binding_;
  std::unique_ptr<cr_fuchsia::LifecycleImpl> lifecycle_;

  // If running as a Mod then these are used to e.g. RemoveSelfFromStory().
  fuchsia::modular::ModuleContextPtr module_context_;

  // Objects used for binding and exporting the ViewProvider service.
  std::unique_ptr<
      base::fuchsia::ScopedServiceBinding<fuchsia::ui::app::ViewProvider>>
      view_provider_binding_;

  // Termination reason and exit-code to be reported via the
  // sys::ComponentController::OnTerminated event.
  fuchsia::sys::TerminationReason termination_reason_ =
      fuchsia::sys::TerminationReason::UNKNOWN;
  int termination_exit_code_ = 0;

  bool view_is_bound_ = false;

  bool component_started_ = false;
  bool enable_remote_debugging_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebComponent);
};

#endif  // FUCHSIA_RUNNERS_COMMON_WEB_COMPONENT_H_
