// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/fake_wk_configuration_provider_observer.h"

#import "ios/web/web_state/ui/wk_web_view_configuration_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace web {
FakeWKConfigurationProviderObserver::FakeWKConfigurationProviderObserver(
    WKWebViewConfigurationProvider* config_provider) {
  config_provider->AddObserver(this);
}

WKWebViewConfiguration*
FakeWKConfigurationProviderObserver::GetLastCreatedWKConfiguration() {
  return last_created_wk_config_;
}

void FakeWKConfigurationProviderObserver::ResetLastCreatedWKConfig() {
  last_created_wk_config_ = nil;
}

void FakeWKConfigurationProviderObserver::DidCreateNewConfiguration(
    WKWebViewConfigurationProvider* config_provider,
    WKWebViewConfiguration* new_config) {
  last_created_wk_config_ = new_config;
}

}  // namespace web
