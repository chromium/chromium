// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_OBSERVER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_OBSERVER_H_

#include "base/observer_list_types.h"

namespace signin {
class IdentityManager;
}

// Observer for IdentityManagerFactory.
class IdentityManagerFactoryObserver : public base::CheckedObserver {
 public:
  IdentityManagerFactoryObserver() {}

  IdentityManagerFactoryObserver(const IdentityManagerFactoryObserver&) =
      delete;
  IdentityManagerFactoryObserver& operator=(
      const IdentityManagerFactoryObserver&) = delete;

  ~IdentityManagerFactoryObserver() override {}

  // Called when an IdentityManager instance is created.
  virtual void IdentityManagerCreated(signin::IdentityManager* manager) {}
};

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_IDENTITY_MANAGER_FACTORY_OBSERVER_H_
