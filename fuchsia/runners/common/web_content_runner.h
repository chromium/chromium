// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
#define FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_

#include <fuchsia/sys/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/macros.h"
#include "base/optional.h"

class WebComponent;

// sys::Runner that instantiates components hosting standard web content.
class WebContentRunner : public fuchsia::sys::Runner,
                         public fuchsia::web::FrameHost {
 public:
  using GetContextParamsCallback =
      base::RepeatingCallback<fuchsia::web::CreateContextParams()>;

  // Creates a Runner which will (re-)create the Context, if not already
  // running, when StartComponent() is called,
  // |get_context_params_callback|: Returns parameters for the Runner's
  //     fuchsia.web.Context.
  explicit WebContentRunner(
      GetContextParamsCallback get_context_params_callback);

  // Creates a Runner using a Context configured with |context_params|.
  // The Runner becomes non-functional if the Context terminates.
  explicit WebContentRunner(fuchsia::web::CreateContextParams context_params);

  ~WebContentRunner() override;

  // fuchsia::web::FrameHost implementation (used by CastRunner).
  // Creates a Frame in this Runner's Context. If no Context exists then
  // |get_context_params_callback_| will be used to create one, if set.
  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> request) override;

  // Used by WebComponent instances to signal that the ComponentController
  // channel was dropped, and therefore the component should be destroyed.
  void DestroyComponent(WebComponent* component);

  // fuchsia::sys::Runner implementation.
  void StartComponent(fuchsia::sys::Package package,
                      fuchsia::sys::StartupInfo startup_info,
                      fidl::InterfaceRequest<fuchsia::sys::ComponentController>
                          controller_request) override;

  // Registers a WebComponent, or specialization, with this Runner.
  void RegisterComponent(std::unique_ptr<WebComponent> component);

  // Sets a callback to invoke when |components_| next becomes empty.
  void SetOnEmptyCallback(base::OnceClosure on_empty);

  // Tears down the Context, if any. This will trigger any active WebComponents
  // to be asynchronously torn-down.
  void DestroyWebContext();

  // TODO(https://crbug.com/1065707): Remove this once capability routing for
  // the fuchsia.legacymetrics.Provider service is properly set up.
  // Returns a pointer to any currently running component, or nullptr if no
  // components are currently running.
  WebComponent* GetAnyComponent();

 private:
  const GetContextParamsCallback get_context_params_callback_;

  // If set, invoked whenever a WebComponent is created.
  base::RepeatingCallback<void(WebComponent*)>
      web_component_created_callback_for_test_;

  fuchsia::web::ContextPtr context_;
  std::set<std::unique_ptr<WebComponent>, base::UniquePtrComparator>
      components_;

  base::OnceClosure on_empty_callback_;

  DISALLOW_COPY_AND_ASSIGN(WebContentRunner);
};

#endif  // FUCHSIA_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
