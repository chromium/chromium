// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_MODEL_GLIC_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_MODEL_GLIC_SERVICE_H_

#import <UIKit/UIKit.h>

#include "components/keyed_service/core/keyed_service.h"

// A browser-context keyed service for glic.
class GlicService : public KeyedService {
 public:
  GlicService();
  ~GlicService() override;

  // Presents the overlay on a given view controller.
  void PresentOverlayOnViewController(UIViewController* base_view_controller);
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_GLIC_MODEL_GLIC_SERVICE_H_
