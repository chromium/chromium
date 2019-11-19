// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_FEATURE_MANAGER_H_
#define IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_FEATURE_MANAGER_H_

#include "base/macros.h"
#include "components/password_manager/core/browser/password_feature_manager.h"

namespace ios_web_view {
// An //ios/web_view implementation of password_manager::PasswordFeatureManager.
class WebViewPasswordFeatureManager
    : public password_manager::PasswordFeatureManager {
 public:
  WebViewPasswordFeatureManager() = default;
  ~WebViewPasswordFeatureManager() override = default;

  bool IsGenerationEnabled() const override;

  bool ShouldCheckReuseOnLeakDetection() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebViewPasswordFeatureManager);
};
}  // namespace ios_web_view

#endif  // IOS_WEB_VIEW_INTERNAL_PASSWORDS_WEB_VIEW_PASSWORD_FEATURE_MANAGER_H_
