// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_H_

#include "components/contextual_search/contextual_search_service.h"

// iOS-specific implementation of ContextualSearchService.
class IOSContextualSearchService
    : public contextual_search::ContextualSearchService {
 public:
  IOSContextualSearchService(
      signin::IdentityManager* identity_manager,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      TemplateURLService* template_url_service,
      variations::VariationsClient* variations_client,
      version_info::Channel channel,
      const std::string& locale);
  ~IOSContextualSearchService() override;

 protected:
  // ContextualSearchService overrides:
  std::unique_ptr<contextual_search::ContextualSearchContextController>
  CreateComposeboxQueryController(
      std::unique_ptr<
          contextual_search::ContextualSearchContextController::ConfigParams>
          query_controller_config_params) override;
};

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MODEL_IOS_CONTEXTUAL_SEARCH_SERVICE_H_
