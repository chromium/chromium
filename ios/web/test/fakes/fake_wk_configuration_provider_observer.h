// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_FAKES_FAKE_WK_CONFIGURATION_PROVIDER_OBSERVER_H_
#define IOS_WEB_TEST_FAKES_FAKE_WK_CONFIGURATION_PROVIDER_OBSERVER_H_

#import "ios/web/web_state/ui/wk_web_view_configuration_provider_observer.h"

#import <WebKit/WebKit.h>

namespace web {
// Fake implementation of WKWebViewConfigurationProviderObserver.
class FakeWKConfigurationProviderObserver
    : public WKWebViewConfigurationProviderObserver {
 public:
  explicit FakeWKConfigurationProviderObserver(
      WKWebViewConfigurationProvider* config_provider);
  // Returns the WKWebViewConfiguration object that was passed to
  // DidCreateNewConfiguration method.
  WKWebViewConfiguration* GetLastCreatedWKConfiguration();

  void ResetLastCreatedWKConfig();

 private:
  // Sets the |last_created_wk_config| with |new_config|.
  void DidCreateNewConfiguration(
      WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) override;

  // The last created configuration that was passed to
  // DidCreateNewConfiguration.
  WKWebViewConfiguration* last_created_wk_config_ = nil;
};

}  // namespace web

#endif  // IOS_WEB_TEST_FAKES_FAKE_WK_CONFIGURATION_PROVIDER_OBSERVER_H_
