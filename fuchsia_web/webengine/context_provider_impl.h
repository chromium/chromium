// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_
#define FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_

#include <fuchsia/web/cpp/fidl.h>

#include "base/values.h"
#include "fuchsia_web/webengine/web_engine_export.h"
#include "fuchsia_web/webinstance_host/web_instance_host.h"

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

  // Sets a config to use for the test, instead of looking for the config file.
  void set_config_for_test(base::Value config);

  // Exposes the fuchsia.web.Debug API to offer to clients.
  fuchsia::web::Debug* debug_api();

 private:
  // Manages an isolated Environment, and the web instances hosted within it.
  WebInstanceHost web_instance_host_;
};

#endif  // FUCHSIA_WEB_WEBENGINE_CONTEXT_PROVIDER_IMPL_H_
