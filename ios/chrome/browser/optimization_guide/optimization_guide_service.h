// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
#define IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_

#include "components/keyed_service/core/keyed_service.h"

// A BrowserState keyed service that is used to own the underlying Optimization
// Guide components.
class OptimizationGuideService : public KeyedService {
 public:
  OptimizationGuideService();
  ~OptimizationGuideService() override;

  OptimizationGuideService(const OptimizationGuideService&) = delete;
  OptimizationGuideService& operator=(const OptimizationGuideService&) = delete;
};

#endif  // IOS_CHROME_BROWSER_OPTIMIZATION_GUIDE_OPTIMIZATION_GUIDE_SERVICE_H_
