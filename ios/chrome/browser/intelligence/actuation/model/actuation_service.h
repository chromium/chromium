// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_H_

#import "components/keyed_service/core/keyed_service.h"

// Service responsible for handling AI actuation requests.
class ActuationService : public KeyedService {
 public:
  ActuationService();
  ~ActuationService() override;

  // KeyedService implementation.
  void Shutdown() override;
};

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTUATION_MODEL_ACTUATION_SERVICE_H_
