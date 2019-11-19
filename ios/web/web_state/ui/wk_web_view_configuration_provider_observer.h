// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_OBSERVER_H_
#define IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_OBSERVER_H_

#include "base/macros.h"

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

  virtual ~WKWebViewConfigurationProviderObserver() = default;

 protected:
  WKWebViewConfigurationProviderObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(WKWebViewConfigurationProviderObserver);
};

}  // namespace web

#endif  // IOS_WEB_WEB_STATE_UI_WK_WEB_VIEW_CONFIGURATION_PROVIDER_OBSERVER_H_
