// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/fuchsia/default_context.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "fuchsia/runners/buildflags.h"
#include "fuchsia/runners/common/web_component.h"
#include "url/gurl.h"

namespace {

fidl::InterfaceHandle<fuchsia::io::Directory> OpenDirectoryOrFail(
    const base::FilePath& path) {
  auto directory = base::fuchsia::OpenDirectory(path);
  CHECK(directory) << "Failed to open " << path;
  return directory;
}

}  // namespace

// static
fuchsia::web::ContextPtr WebContentRunner::CreateWebContext(
    fuchsia::web::CreateContextParams create_params) {
  auto web_context_provider = base::fuchsia::ComponentContextForCurrentProcess()
                                  ->svc()
                                  ->Connect<fuchsia::web::ContextProvider>();

  fuchsia::web::ContextPtr web_context;
  web_context_provider->Create(std::move(create_params),
                               web_context.NewRequest());
  web_context.set_error_handler([](zx_status_t status) {
    // If the browser instance died, then exit everything and do not attempt
    // to recover. appmgr will relaunch the runner when it is needed again.
    ZX_LOG(ERROR, status) << "Connection to Context lost.";
    exit(1);
  });
  return web_context;
}

// static
fuchsia::web::ContextPtr WebContentRunner::CreateDefaultWebContext(
    fuchsia::web::ContextFeatureFlags features) {
  fuchsia::web::CreateContextParams create_context_params =
      BuildCreateContextParams(OpenDirectoryOrFail(base::FilePath(
                                   base::fuchsia::kPersistedDataDirectoryPath)),
                               features);

  if (BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT) != 0) {
    create_context_params.set_remote_debugging_port(
        BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT));
  }

  return CreateWebContext(std::move(create_context_params));
}

// static
fuchsia::web::CreateContextParams WebContentRunner::BuildCreateContextParams(
    fidl::InterfaceHandle<fuchsia::io::Directory> data_directory,
    fuchsia::web::ContextFeatureFlags features) {
  fuchsia::web::CreateContextParams create_params;
  create_params.set_service_directory(OpenDirectoryOrFail(
      base::FilePath(base::fuchsia::kServiceDirectoryPath)));

  if (data_directory)
    create_params.set_data_directory(std::move(data_directory));

  create_params.set_features(features);

  return create_params;
}

WebContentRunner::WebContentRunner(
    sys::OutgoingDirectory* outgoing_directory,
    CreateContextCallback create_context_callback)
    : create_context_callback_(std::move(create_context_callback)) {
  DCHECK(create_context_callback_);
  service_binding_.emplace(outgoing_directory, this);
}

WebContentRunner::WebContentRunner(fuchsia::web::ContextPtr context)
    : context_(std::move(context)) {}

WebContentRunner::~WebContentRunner() = default;

fuchsia::web::Context* WebContentRunner::GetContext() {
  if (!context_) {
    DCHECK(create_context_callback_);
    context_ = std::move(create_context_callback_).Run();
    DCHECK(context_);
  }

  return context_.get();
}

void WebContentRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  GURL url(package.resolved_url);
  if (!url.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url;
    return;
  }

  std::unique_ptr<WebComponent> component = std::make_unique<WebComponent>(
      this,
      std::make_unique<base::fuchsia::StartupContext>(std::move(startup_info)),
      std::move(controller_request));
  if (BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT) != 0)
    component->EnableRemoteDebugging();
  component->StartComponent();
  component->LoadUrl(url, std::vector<fuchsia::net::http::Header>());
  RegisterComponent(std::move(component));
}

void WebContentRunner::SetWebComponentCreatedCallbackForTest(
    base::RepeatingCallback<void(WebComponent*)> callback) {
  DCHECK(components_.empty());
  web_component_created_callback_for_test_ = std::move(callback);
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  if (web_component_created_callback_for_test_)
    web_component_created_callback_for_test_.Run(component.get());

  components_.insert(std::move(component));
}
