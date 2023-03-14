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

#include <utility>

#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/guid.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/sequence_checker.h"
#include "fuchsia_web/webinstance_host/fuchsia_web_debug_proxy.h"

namespace fuchsia::io {
class Directory;
}

namespace sys {
class OutgoingDirectory;
}

// Helper class that allows web_instance Components to be launched based on
// caller-supplied |CreateContextParams|.
//
// Note that Components using this class must:
// 1. Include `web_instance_host.shard.cml` in their component manifest.
// 2. Have web_instance's config-data available to the calling Component as
//    the `config-data-for-web-instance` directory capability.
//
// To ensure proper product data registration, Components using the class must:
// * Have the same version and channel as WebEngine.
// * Instantiate the class on a thread with an async_dispatcher.
// TODO(crbug.com/1211174): Remove these requirements.
class WebInstanceHost {
 public:
  // The host will offer capabilities to child instances via
  // `outgoing_directory`. WebInstanceHost owners must serve the directory
  // before creating web instances, and must ensure that the directory outlives
  // the WebInstanceHost instance.
  // TODO(crbug.com/1327587): Remove `outgoing_directory` if and when it is
  // possible for tests to serve a test-specific outgoing directory via
  // base::TestComponentContextForProcess on a separate thread.
  explicit WebInstanceHost(sys::OutgoingDirectory& outgoing_directory);
  ~WebInstanceHost();

  WebInstanceHost(const WebInstanceHost&) = delete;
  WebInstanceHost& operator=(const WebInstanceHost&) = delete;

  // Creates a new web_instance Component and connects |services_request| to it.
  // Returns ZX_OK if |params| were valid, and the Component was launched.
  // |extra_args| are included on the command line when launching the new
  // web_instance. Use base::CommandLine(base::CommandLine::NO_PROGRAM) for
  // empty args.
  zx_status_t CreateInstanceForContextWithCopiedArgs(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::io::Directory> services_request,
      base::CommandLine extra_args);

  // Exposes a fuchsia.web.Debug protocol implementation that can be used
  // to receive notifications of DevTools debug ports for new web instances.
  fuchsia::web::Debug& debug_api() { return debug_proxy_; }

  // The next created WebInstance will have access to the given directory handle
  // for temporary directory reading and writing.
  // Ownership of the directory is passed to the next created instance.
  void set_tmp_dir(fidl::InterfaceHandle<fuchsia::io::Directory> tmp_dir) {
    tmp_dir_ = std::move(tmp_dir);
  }

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
  void OnComponentBinderClosed(const base::GUID& id, zx_status_t status);

  // The directory via which directory capabilities are dynamically provided to
  // child instances.
  const raw_ref<sys::OutgoingDirectory> outgoing_directory_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // The framework-provided protocol used to manage child instances.
  fidl::InterfacePtr<fuchsia::component::Realm> realm_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // A mapping of child instance GUID to the child's Binder interface, by which
  // child instance shutdown is observed.
  base::flat_map<base::GUID, fidl::InterfacePtr<fuchsia::component::Binder>>
      instances_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Implements the fuchsia.web.Debug API across all instances.
  FuchsiaWebDebugProxy debug_proxy_;

  // If set, then the next created WebInstance will gain ownership of this
  // directory.
  fidl::InterfaceHandle<fuchsia::io::Directory> tmp_dir_;

  SEQUENCE_CHECKER(sequence_checker_);
};

#endif  // FUCHSIA_WEB_WEBINSTANCE_HOST_WEB_INSTANCE_HOST_H_
