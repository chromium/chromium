// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/composebox/model/ios_contextual_search_service.h"

#include "components/omnibox/composebox/ios/composebox_query_controller_ios.h"

IOSContextualSearchService::IOSContextualSearchService(
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    TemplateURLService* template_url_service,
    variations::VariationsClient* variations_client,
    version_info::Channel channel,
    const std::string& locale)
    : contextual_search::ContextualSearchService(identity_manager,
                                                 url_loader_factory,
                                                 template_url_service,
                                                 variations_client,
                                                 channel,
                                                 locale) {}

IOSContextualSearchService::~IOSContextualSearchService() = default;

std::unique_ptr<contextual_search::ContextualSearchContextController>
IOSContextualSearchService::CreateComposeboxQueryController(
    std::unique_ptr<
        contextual_search::ContextualSearchContextController::ConfigParams>
        query_controller_config_params) {
  return std::make_unique<ComposeboxQueryControllerIOS>(
      identity_manager_, url_loader_factory_, channel_, locale_,
      template_url_service_, variations_client_,
      std::move(query_controller_config_params));
}
