// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/scoped_web_ui_controller_factory_registration.h"

#include "base/strings/strcat.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/browser/webui_config.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/common/url_constants.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

GURL GetUrlFromConfig(WebUIConfig& webui_config) {
  return GURL(
      base::StrCat({webui_config.scheme(), url::kStandardSchemeSeparator,
                    webui_config.host()}));
}

void AddWebUIConfig(std::unique_ptr<WebUIConfig> webui_config) {
  auto& config_map = WebUIConfigMap::GetInstance();

  if (webui_config->scheme() == kChromeUIScheme) {
    config_map.AddWebUIConfig(std::move(webui_config));
    return;
  }

  if (webui_config->scheme() == kChromeUIUntrustedScheme) {
    config_map.AddUntrustedWebUIConfig(std::move(webui_config));
    return;
  }

  NOTREACHED_IN_MIGRATION();
}

}  // namespace

ScopedWebUIControllerFactoryRegistration::
    ScopedWebUIControllerFactoryRegistration(
        content::WebUIControllerFactory* factory,
        content::WebUIControllerFactory* factory_to_replace)
    : factory_(factory), factory_to_replace_(factory_to_replace) {
  if (factory_to_replace_) {
    content::WebUIControllerFactory::UnregisterFactoryForTesting(
        factory_to_replace_);
  }
  content::WebUIControllerFactory::RegisterFactory(factory_);
}

ScopedWebUIControllerFactoryRegistration::
    ~ScopedWebUIControllerFactoryRegistration() {
  content::WebUIControllerFactory::UnregisterFactoryForTesting(factory_);
  // If we unregistered a registered factory, re-register it to keep global
  // state clean for future tests.
  if (factory_to_replace_)
    content::WebUIControllerFactory::RegisterFactory(factory_to_replace_);
}

ScopedWebUIConfigRegistration::ScopedWebUIConfigRegistration(
    std::unique_ptr<WebUIConfig> webui_config)
    : webui_config_url_(GetUrlFromConfig(*webui_config)) {
  auto& config_map = WebUIConfigMap::GetInstance();
  replaced_webui_config_ = config_map.RemoveConfig(webui_config_url_);

  DCHECK(webui_config.get() != nullptr);
  AddWebUIConfig(std::move(webui_config));
}

ScopedWebUIConfigRegistration::ScopedWebUIConfigRegistration(
    const GURL& webui_origin)
    : webui_config_url_(webui_origin) {
  auto& config_map = WebUIConfigMap::GetInstance();
  replaced_webui_config_ = config_map.RemoveConfig(webui_config_url_);
}

ScopedWebUIConfigRegistration::~ScopedWebUIConfigRegistration() {
  WebUIConfigMap::GetInstance().RemoveConfig(webui_config_url_);

  // If we replaced a WebUIConfig, re-register it to keep the global state
  // clean for future tests.
  if (replaced_webui_config_ != nullptr)
    AddWebUIConfig(std::move(replaced_webui_config_));
}

void CheckForLeakedWebUIRegistrations::OnTestStart(
    const testing::TestInfo& test_info) {
  // Call GetInstance() to ensure WebUIConfig registers its
  // WebUIControllerFactory before we get the number of registered factories.
  initial_size_of_webui_config_map_ =
      WebUIConfigMap::GetInstance().GetWebUIConfigList(nullptr).size();
  initial_num_factories_registered_ =
      content::WebUIControllerFactory::GetNumRegisteredFactoriesForTesting();
}

void CheckForLeakedWebUIRegistrations::OnTestEnd(
    const testing::TestInfo& test_info) {
  EXPECT_EQ(initial_size_of_webui_config_map_,
            WebUIConfigMap::GetInstance().GetWebUIConfigList(nullptr).size())
      << "A WebUIConfig was registered by a test but never unregistered. This "
         "can cause flakiness in later tests. Please use "
         "ScopedWebUIConfigRegistration to ensure that registered configs are "
         "unregistered.";

  EXPECT_EQ(
      initial_num_factories_registered_,
      content::WebUIControllerFactory::GetNumRegisteredFactoriesForTesting())
      << "A WebUIControllerFactory was registered by a test but never "
         "unregistered. This can cause flakiness in later tests. Please use "
         "ScopedWebUIControllerFactoryRegistration to ensure that registered "
         "factories are unregistered.";
}

}  // namespace content
