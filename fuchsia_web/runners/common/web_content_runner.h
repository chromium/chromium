// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
#define FUCHSIA_WEB_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_

#include <fuchsia/io/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr_set.h>

#include <memory>
#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "base/time/time.h"

class WebComponent;

// Manages creation of a `fuchsia.web.Context` used to host content for one or
// more `WebComponent`s.
class WebContentRunner {
 public:
  struct WebInstanceConfig {
    WebInstanceConfig();
    ~WebInstanceConfig();

    WebInstanceConfig(WebInstanceConfig&&);
    WebInstanceConfig& operator=(WebInstanceConfig&&);

    fuchsia::web::CreateContextParams params;
    base::CommandLine extra_args;
  };

  using CreateWebInstanceAndContextCallback =
      base::RepeatingCallback<zx_status_t(
          fuchsia::web::CreateContextParams params,
          fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
          const base::CommandLine& extra_args)>;

  using GetWebInstanceConfigCallback =
      base::RepeatingCallback<WebInstanceConfig()>;

  // Creates a Runner which will (re-)create the Context, if not already
  // running, when StartComponent() is called.
  // |create_web_instance_callback|: Used to create a web_instance Component in
  //     which to host the fuchsia.web.Context.
  // |get_web_instance_config_callback|: Returns parameters for the Runner's
  //     fuchsia.web.Context.
  WebContentRunner(
      CreateWebInstanceAndContextCallback create_web_instance_callback,
      GetWebInstanceConfigCallback get_web_instance_config_callback);

  // Creates a Runner using a Context configured with `web_instance_config`.
  // The Runner becomes non-functional if the Context terminates.
  WebContentRunner(
      CreateWebInstanceAndContextCallback create_web_instance_callback,
      WebInstanceConfig web_instance_config);

  ~WebContentRunner();

  WebContentRunner(const WebContentRunner&) = delete;
  WebContentRunner& operator=(const WebContentRunner&) = delete;

  // Returns a request handler for fuchsia.web.FrameHost protocol requests.
  // FrameHost instances will be run in the same web_instance as the Context
  // used to host WebComponent's Frames.
  // If no web_instance is currently running then
  // |get_web_instance_config_callback_| will be used to create one with the
  // specified parameters.
  fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
  GetFrameHostRequestHandler();

  // Used by WebComponent to create a Frame in this Runner's web_instance.
  // If no web_instance is active then |get_web_instance_config_callback_| will
  // be used to create one, if set (see class constructors).
  void CreateFrameWithParams(
      fuchsia::web::CreateFrameParams params,
      fidl::InterfaceRequest<fuchsia::web::Frame> request);

  // Used by WebComponent instances to signal that the ComponentController
  // channel was dropped, and therefore the component should be destroyed.
  void DestroyComponent(WebComponent* component);

  // Registers a WebComponent, or specialization, with this Runner.
  void RegisterComponent(std::unique_ptr<WebComponent> component);

  // Sets a callback to invoke when |components_| next becomes empty.
  void SetOnEmptyCallback(base::OnceClosure on_empty);

  // Tears down the Context, if any. This will trigger any active WebComponents
  // to be asynchronously torn-down.
  void DestroyWebContext();

  // Signals to the `Frame` to close the page within the specified `timeout`,
  // retaining the channel until it closes itself.
  void CloseFrameWithTimeout(fuchsia::web::FramePtr frame,
                             base::TimeDelta timeout);

 private:
  // Ensures that there is a web_instance Component running, and connects
  // |context_| to it.
  void EnsureWebInstanceAndContext();

  // Starts the web_instance and connects |context_| to it.
  void CreateWebInstanceAndContext(WebInstanceConfig web_instance_config);

  const CreateWebInstanceAndContextCallback create_web_instance_callback_;
  const GetWebInstanceConfigCallback get_web_instance_config_callback_;

  // If set, invoked whenever a WebComponent is created.
  base::RepeatingCallback<void(WebComponent*)>
      web_component_created_callback_for_test_;

  fuchsia::web::ContextPtr context_;
  fuchsia::io::DirectoryHandle web_instance_services_;
  std::set<std::unique_ptr<WebComponent>, base::UniquePtrComparator>
      components_;

  // Retains `Frame`s belonging to components for which `CloseFrameWithTimeout`
  // was called, to allow them to out-live their owning component.
  fidl::InterfacePtrSet<fuchsia::web::Frame> closing_frames_;

  base::OnceClosure on_empty_callback_;
};

#endif  // FUCHSIA_WEB_RUNNERS_COMMON_WEB_CONTENT_RUNNER_H_
