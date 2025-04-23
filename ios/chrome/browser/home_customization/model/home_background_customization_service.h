// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_

#import "components/keyed_service/core/keyed_service.h"

// Service for allowing customization of the Home surface background.
class HomeBackgroundCustomizationService : public KeyedService {
 public:
  explicit HomeBackgroundCustomizationService();

  HomeBackgroundCustomizationService(
      const HomeBackgroundCustomizationService&) = delete;
  HomeBackgroundCustomizationService& operator=(
      const HomeBackgroundCustomizationService&) = delete;

  ~HomeBackgroundCustomizationService() override;

  // KeyedService implementation:
  void Shutdown() override;
};

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_MODEL_HOME_BACKGROUND_CUSTOMIZATION_SERVICE_H_
