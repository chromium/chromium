// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_COMMON_WEB_COMPONENT_H_
#define FUCHSIA_WEB_RUNNERS_COMMON_WEB_COMPONENT_H_

#include <fuchsia/modular/cpp/fidl.h>
#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/ui/app/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/binding.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/time/time.h"
#include "fuchsia_web/runners/common/modular/lifecycle_impl.h"
#include "url/gurl.h"

class WebContentRunner;

// Base component implementation for web-based content Runners. Each instance
// manages the lifetime of its own fuchsia::web::Frame, including associated
// resources and service bindings.  Runners for specialized web-based content
// (e.g. Cast applications) can extend this class to configure the Frame to
// their needs, publish additional APIs, etc.
class WebComponent : public fuchsia::sys::ComponentController,
                     public fuchsia::ui::app::ViewProvider,
                     public fuchsia::web::NavigationEventListener {
 public:
  // Creates a WebComponent encapsulating a web.Frame.
  // |debug_name| may be empty, or specified a name to use to uniquely identify
  //   the Frame in log output.
  // |runner| must out-live |this|.
  // [context| will be retained to provide component-specific services.
  //   If |context| includes an outgoing-directory request then the component
  //   will publish ViewProvider and Lifecycle services.
  // |controller_request| may optionally be supplied and used to control the
  //   lifetime of this component instance.
  WebComponent(base::StringPiece debug_name,
               WebContentRunner* runner,
               std::unique_ptr<base::StartupContext> context,
               fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                   controller_request);

  WebComponent(const WebComponent&) = delete;
  WebComponent& operator=(const WebComponent&) = delete;

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

 protected:
  fuchsia::web::Frame* frame() const { return frame_.get(); }

  // Returns the component's startup context (e.g. incoming services, public
  // service directory, etc).
  base::StartupContext* startup_context() const {
    return startup_context_.get();
  }

  // fuchsia::sys::ComponentController implementation.
  void Kill() override;
  void Detach() override;

  // fuchsia::ui::app::ViewProvider implementation.
  void CreateView(
      zx::eventpair view_token,
      fidl::InterfaceRequest<fuchsia::sys::ServiceProvider> incoming_services,
      fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> outgoing_services)
      override;
  void CreateViewWithViewRef(zx::eventpair view_token,
                             fuchsia::ui::views::ViewRefControl control_ref,
                             fuchsia::ui::views::ViewRef view_ref) override;
  void CreateView2(fuchsia::ui::app::CreateView2Args view_args) override;

  // fuchsia::web::NavigationEventListener implementation.
  // Used to detect when the Frame enters an error state (e.g. the top-level
  // content's Renderer process crashes).
  void OnNavigationStateChanged(
      fuchsia::web::NavigationState change,
      OnNavigationStateChangedCallback callback) override;

  // Reports the supplied exit-code and reason to the `controller_binding_` and
  // requests that the `runner_` delete this component.
  // `reason` should be set to EXITED for expected terminations, in which case
  // `exit_code` should be set to a `zx_status_t` value, e.g. the status
  // reported by the error-handler for the component's `Frame`.
  virtual void DestroyComponent(int64_t exit_code,
                                fuchsia::sys::TerminationReason reason);

  // Invokes `Close()` on `frame_` with the specified `timeout`.
  void CloseFrameWithTimeout(base::TimeDelta timeout);

 private:
  // Optional name with which to tag console log output from the Frame.
  const std::string debug_name_;

  // Refers to the owner of the web.Context hosting this component instance.
  WebContentRunner* const runner_ = nullptr;

  // Component context for this instance, including incoming services.
  const std::unique_ptr<base::StartupContext> startup_context_;

  fuchsia::web::FramePtr frame_;

  // Bindings used to manage the lifetime of this component instance.
  fidl::Binding<fuchsia::sys::ComponentController> controller_binding_;
  std::unique_ptr<cr_fuchsia::LifecycleImpl> lifecycle_;

  // If running as a Mod then these are used to e.g. RemoveSelfFromStory().
  fuchsia::modular::ModuleContextPtr module_context_;

  // Objects used for binding and exporting the ViewProvider service.
  std::unique_ptr<base::ScopedServiceBinding<fuchsia::ui::app::ViewProvider>>
      view_provider_binding_;

  // Termination reason and exit-code to be reported via the
  // sys::ComponentController::OnTerminated event.
  fuchsia::sys::TerminationReason termination_reason_ =
      fuchsia::sys::TerminationReason::UNKNOWN;
  int64_t termination_exit_code_ = 0;

  bool view_is_bound_ = false;

  bool component_started_ = false;
  bool enable_remote_debugging_ = false;

  // Used to watch for failures of the Frame's web content, including Renderer
  // process crashes.
  fidl::Binding<fuchsia::web::NavigationEventListener>
      navigation_listener_binding_;
};

#endif  // FUCHSIA_WEB_RUNNERS_COMMON_WEB_COMPONENT_H_
