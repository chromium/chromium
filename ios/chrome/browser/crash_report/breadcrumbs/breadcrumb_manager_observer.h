// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_H_

#include <string>

#include "base/macros.h"
#include "base/observer_list_types.h"

class BreadcrumbManager;

class BreadcrumbManagerObserver : public base::CheckedObserver {
 public:
  // Called when a new |event| has been added to |manager|. Similar to
  // |BreadcrumbManager::GetEvents|, |event| will have the timestamp at which it
  // was logged prepended to the string which was passed to
  // |BreadcrumbManager::AddEvent|.
  virtual void EventAdded(BreadcrumbManager* manager,
                          const std::string& event) {}

 protected:
  BreadcrumbManagerObserver() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(BreadcrumbManagerObserver);
};

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_BREADCRUMBS_BREADCRUMB_MANAGER_OBSERVER_H_
