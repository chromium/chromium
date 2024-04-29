// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_VARIATIONS_SERVICE_H_
#define IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_VARIATIONS_SERVICE_H_

#import "components/variations/service/variations_service.h"
#import "components/variations/synthetic_trial_registry.h"

// Creates a VariationsService and sets it as the TestingApplicationContext's
// VariationService for the life of the instance.
class IOSChromeScopedTestingVariationsService {
 public:
  IOSChromeScopedTestingVariationsService();

  ~IOSChromeScopedTestingVariationsService();

  variations::VariationsService* Get();

  std::unique_ptr<variations::VariationsService> variations_service_;
  std::unique_ptr<variations::SyntheticTrialRegistry> synthetic_trial_registry_;
};

#endif  // IOS_CHROME_TEST_IOS_CHROME_SCOPED_TESTING_VARIATIONS_SERVICE_H_
