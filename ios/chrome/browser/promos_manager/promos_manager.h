// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_

#import "base/values.h"
#import "components/prefs/pref_service.h"

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams interested in displaying promos should
// leverage this manager.
class PromosManager {
 public:
  explicit PromosManager(PrefService* local_state);
  ~PromosManager();

  // Initialize the Promos Manager by restoring state from Prefs. Must be called
  // after creation and before any other operation.
  void Init();

 private:
  // Weak pointer to the local state prefs store.
  const raw_ptr<PrefService> local_state_;

  // base::Value::List of active promos.
  base::Value::List active_promos_;

  // base::Value::Dict of the promo impression history.
  base::Value::Dict impression_history_;
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
