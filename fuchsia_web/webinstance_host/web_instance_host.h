// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_
#define FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_

#include <fuchsia/component/cpp/fidl.h>
#include <fuchsia/web/cpp/fidl.h>
#include <lib/fidl/cpp/interface_ptr.h>
#include <lib/fidl/cpp/interface_request.h>
#include <zircon/types.h>

#include <string_view>
#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "base/uuid.h"
#include "fuchsia_web/webinstance_host/fuchsia_web_debug_proxy.h"

namespace fuchsia::io {
class Directory;
}

namespace sys {
class OutgoingDirectory;
}

// Helper class that allows web_instance Components to be launched based on
// caller-supplied |CreateContextParams|.
// Use one of the concrete subclasses when instantiating.
//
// Note that Components using this class must:
// 1. Include `web_instance_host.shard.cml` in their component manifest.
// 2. Have web_instance's config-data available to the calling Component as
//    the `config-data-for-web-instance` directory capability.
//
// To ensure proper product data registration, Components using the class must:
// * Have the same version and channel as WebEngine.
// * Instantiate the class on a thread with an async_dispatcher.
// TODO(crbug.com/42050393): Remove these requirements when platform supports
// it.
class WebInstanceHost {
 public:
  virtual ~WebInstanceHost();

  WebInstanceHost(const WebInstanceHost&) = delete;
  WebInstanceHost& operator=(const WebInstanceHost&) = delete;

  // Creates a new web_instance Component and connects |services_request| to it.
  // Returns ZX_OK if `params` were valid, and the Component was launched.
  // `extra_args` are included on the command line when launching the new
  // web_instance. Use `base::CommandLine(base::CommandLine::NO_PROGRAM)` for
  // empty args.
  virtual zx_status_t CreateInstanceForContextWithCopiedArgs(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      const base::CommandLine& extra_args) = 0;

  // Exposes a fuchsia.web.Debug protocol implementation that can be used
  // to receive notifications of DevTools debug ports for new web instances.
  fuchsia::web::Debug& debug_api() { return debug_proxy_; }

  // The next created WebInstance will have access to the given directory handle
  // for temporary directory reading and writing.
  // Ownership of the directory is passed to the next created instance.
  void set_tmp_dir(fidl::InterfaceHandle<fuchsia::io::Directory> tmp_dir) {
    tmp_dir_ = std::move(tmp_dir);
  }

 protected:
  // The host will offer capabilities to child instances via
  // `outgoing_directory`. WebInstanceHost owners must serve the directory
  // before creating web instances, and must ensure that the directory outlives
  // the WebInstanceHost instance.
  // TODO(crbug.com/40841277): Remove `outgoing_directory` if and when it is
  // possible for tests to serve a test-specific outgoing directory via
  // base::TestComponentContextForProcess on a separate thread.
  WebInstanceHost(sys::OutgoingDirectory& outgoing_directory,
                  bool is_web_instance_component_in_same_package);

  // Creates a new web_instance Component using `instance_component_url` and
  // connects |services_request| to it. Returns ZX_OK if `params` were valid,
  // and the Component was launched. `extra_args` are included on the command
  // line when launching the new web_instance. Use
  // `base::CommandLine(base::CommandLine::NO_PROGRAM)` for empty args.
  // `services_to_offer` must be non-empty if and only if
  // `params.service_directory` is present
  zx_status_t CreateInstanceForContextWithCopiedArgsAndUrl(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      base::CommandLine extra_args,
      std::string_view instance_component_url,
      std::vector<std::string> services_to_offer);

 private:
  bool is_initialized() const VALID_CONTEXT_REQUIRED(sequence_checker_) {
    return realm_.is_bound();
  }

  // Connects to the fuchsia.component/Realm protocol.
  void Initialize();

  // Destroys all child instances and associated resources and unbinds from the
  // fuchsia.component/Realm protocol.
  void Uninitialize();

  // Error handler for the channel to RealmBuilder.
  void OnRealmError(zx_status_t status);

  // Error handler for the channel to an instance's Binder.
  void OnComponentBinderClosed(const base::Uuid& id, zx_status_t status);

  // The directory via which directory capabilities are dynamically provided to
  // child instances.
  const raw_ref<sys::OutgoingDirectory> outgoing_directory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The framework-provided protocol used to manage child instances.
  fidl::InterfacePtr<fuchsia::component::Realm> realm_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A mapping of child instance GUID to the child's Binder interface, by which
  // child instance shutdown is observed.
  base::flat_map<base::Uuid, fidl::InterfacePtr<fuchsia::component::Binder>>
      instances_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Implements the fuchsia.web.Debug API across all instances.
  FuchsiaWebDebugProxy debug_proxy_;

  // If set, then the next created WebInstance will gain ownership of this
  // directory.
  fidl::InterfaceHandle<fuchsia::io::Directory> tmp_dir_;

  // Whether `web_instance.cm` is in the same Package as this host Component.
  // TODO(crbug.com/42050363): Determine this based on a static Structured
  // Configuration value once Structured Configuration is supported.
  const bool is_web_instance_component_in_same_package_;

  SEQUENCE_CHECKER(sequence_checker_);
};

// An instantiable WebInstanceHost where services from this Component are
// provided to each `WebInstance`.
class WebInstanceHostWithServicesFromThisComponent : public WebInstanceHost {
 public:
  WebInstanceHostWithServicesFromThisComponent(
      sys::OutgoingDirectory& outgoing_directory,
      bool is_web_instance_component_in_same_package);

  zx_status_t CreateInstanceForContextWithCopiedArgs(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      const base::CommandLine& extra_args) override;
};

// An instantiable WebInstanceHost where services must be provided for each
// `WebInstance` in `params.service_directory` in the call to
// `CreateInstanceForContextWithCopiedArgs()`.
class WebInstanceHostWithoutServices : public WebInstanceHost {
 public:
  WebInstanceHostWithoutServices(
      sys::OutgoingDirectory& outgoing_directory,
      bool is_web_instance_component_in_same_package);

  zx_status_t CreateInstanceForContextWithCopiedArgs(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      const base::CommandLine& extra_args) override;
};

#endif  // FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_
