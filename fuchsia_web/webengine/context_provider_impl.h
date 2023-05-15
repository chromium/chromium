// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia_web/webengine/web_engine_export.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"

namespace sys {
class OutgoingDirectory;
}  // namespace sys

class WEB_ENGINE_EXPORT ContextProviderImpl
    : public fuchsia::web::ContextProvider {
 public:
  // The impl will offer capabilities to child instances via
  // `outgoing_directory`. ContextProviderImpl owners must serve the directory
  // before creating web instances, and must ensure that the directory outlives
  // the ContextProviderImpl instance.
  explicit ContextProviderImpl(sys::OutgoingDirectory& outgoing_directory);
  ~ContextProviderImpl() override;

  ContextProviderImpl(const ContextProviderImpl&) = delete;
  ContextProviderImpl& operator=(const ContextProviderImpl&) = delete;

  // fuchsia::web::ContextProvider implementation.
  void Create(
      fuchsia::web::CreateContextParams params,
      fidl::InterfaceRequest<fuchsia::web::Context> context_request) override;

  // Exposes the fuchsia.web.Debug API to offer to clients.
  fuchsia::web::Debug* debug_api();

 private:
  // Manages an isolated Environment, and the web instances hosted within it.
  // Services for each web instance are provided by the Service Directory
  // provided in the Create() call.
  WebInstanceHostWithoutServices web_instance_host_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_
