// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_OBSERVER_H_
#define IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_OBSERVER_H_

@class WKWebViewConfiguration;

namespace web {

class WKWebViewConfigurationProvider;

class WKWebViewConfigurationProviderObserver {
 public:
  // Called when the observed WKWebViewConfigurationProvider creates a new
  // WKWebViewConfiguration.
  virtual void DidCreateNewConfiguration(
      WKWebViewConfigurationProvider* config_provider,
      WKWebViewConfiguration* new_config) {}

  WKWebViewConfigurationProviderObserver(
      const WKWebViewConfigurationProviderObserver&) = delete;
  WKWebViewConfigurationProviderObserver& operator=(
      const WKWebViewConfigurationProviderObserver&) = delete;

  virtual ~WKWebViewConfigurationProviderObserver() = default;

 protected:
  WKWebViewConfigurationProviderObserver() = default;
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_OBSERVER_H_
