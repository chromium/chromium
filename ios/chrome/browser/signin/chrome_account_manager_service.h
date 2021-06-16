// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_
#define IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_

#import <Foundation/Foundation.h>

#include "components/keyed_service/core/keyed_service.h"

class PrefService;

// Service that provides Chrome identities.
class ChromeAccountManagerService : public KeyedService {
 public:
  // Initializes the service.
  explicit ChromeAccountManagerService(PrefService* pref_service);
  ChromeAccountManagerService(const ChromeAccountManagerService&) = delete;
  ChromeAccountManagerService& operator=(const ChromeAccountManagerService&) =
      delete;

  // KeyedService implementation.
  void Shutdown() override;
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_CHROME_ACCOUNT_MANAGER_SERVICE_H_
