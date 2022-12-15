// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "fuchsia_web/webengine/web_engine_export.h"
#include "fuchsia_web/webinstance_host/web_instance_host_v1.h"

class WEB_ENGINE_EXPORT ContextProviderImpl
    : public fuchsia::web::ContextProvider {
 public:
  ContextProviderImpl();
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
  WebInstanceHostV1 web_instance_host_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_
