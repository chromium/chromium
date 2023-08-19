// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/context_provider_impl.h"

#include <lib/sys/cpp/service_directory.h>
#include <utility>

#include "base/command_line.h"
#include "base/logging.h"

ContextProviderImpl::ContextProviderImpl(
    sys::OutgoingDirectory& outgoing_directory)
    : web_instance_host_(outgoing_directory,
                         /*is_web_instance_component_in_same_package=*/true) {}

ContextProviderImpl::~ContextProviderImpl() = default;

void ContextProviderImpl::Create(
    fuchsia::web::CreateContextParams params,
    fidl::InterfaceRequest<fuchsia::web::Context> context_request) {
  if (!context_request.is_valid()) {
    DLOG(ERROR) << "Invalid |context_request|.";
    return;
  }

  // The CreateInstanceForContextWithCopiedArgs() call below requires that
  // `params` has a service directory.
  if (!params.has_service_directory()) {
    context_request.Close(ZX_ERR_INVALID_ARGS);
    return;
  }

  // Create the instance and request access to its outgoing service directory.
  fidl::InterfaceHandle<fuchsia::io::Directory> services_handle;
  zx_status_t result =
      web_instance_host_.CreateInstanceForContextWithCopiedArgs(
          std::move(params), services_handle.NewRequest(),
          *base::CommandLine::ForCurrentProcess());

  if (result == ZX_OK) {
    sys::ServiceDirectory services(services_handle.Bind());

    // Route the fuchsia.web.Context request to the new Component.
    services.Connect(std::move(context_request));
  } else {
    context_request.Close(result);
  }
}

fuchsia::web::Debug* ContextProviderImpl::debug_api() {
  return &web_instance_host_.debug_api();
}
