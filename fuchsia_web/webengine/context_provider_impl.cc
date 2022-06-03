// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/context_provider_impl.h"

#include <lib/sys/cpp/service_directory.h>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"
#include "fuchsia_web/webengine/fidl/chromium/internal/cpp/fidl.h"

ContextProviderImpl::ContextProviderImpl() = default;

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::Create(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::web::Context> context_request) {
  if (!context_request.is_valid()) {
    DLOG(ERROR) << "Invalid |context_request|.";
    return;
  }

  // Request access to the component's outgoing service directory.
  fidl::InterfaceRequest<fuchsia::io::Directory> services_request;
  auto services = sys::ServiceDirectory::CreateWithRequest(&services_request);

  // If there are DevToolsListeners active then set the remote-debugging option
  // and create DevToolsPerContextListener channels to connect asynchronously
  // to the instance.
  const bool have_devtools_listeners = devtools_listeners_.size() > 0;
  web_instance_host_.set_enable_remote_debug_mode(have_devtools_listeners);
  if (have_devtools_listeners) {
    chromium::internal::DevToolsConnectorPtr devtools_connector;
    services->Connect(devtools_connector.NewRequest());
    for (auto& devtools_listener : devtools_listeners_.ptrs()) {
      fidl::InterfaceHandle<fuchsia::web::DevToolsPerContextListener> listener;
      devtools_listener.get()->get()->OnContextDevToolsAvailable(
          listener.NewRequest());
      devtools_connector->ConnectPerContextListener(std::move(listener));
    }
  }

  zx_status_t result =
      web_instance_host_.CreateInstanceForContextWithCopiedArgs(
          std::move(params), std::move(services_request),
          *base::CommandLine::ForCurrentProcess());

  if (result == ZX_OK) {
    // Route the fuchsia.web.Context request to the new Component.
    services->Connect(std::move(context_request));
  } else {
    context_request.Close(result);
  }
}

void ContextProviderImpl::set_config_for_test(base::Value config) {
  web_instance_host_.set_config_for_test(std::move(config));  // IN-TEST
}

void ContextProviderImpl::EnableDevTools(
    fidl::InterfaceHandle<fuchsia::web::DevToolsListener> listener,
    EnableDevToolsCallback callback) {
  devtools_listeners_.AddInterfacePtr(listener.Bind());
  callback();
}
