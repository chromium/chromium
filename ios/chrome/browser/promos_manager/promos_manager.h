// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
#define IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_

// Centralized promos manager for coordinating and scheduling the display of
// app-wide promos. Feature teams interested in displaying promos should
// leverage this manager.
class PromosManager {
 public:
  PromosManager();
  ~PromosManager();
};

#endif  // IOS_CHROME_BROWSER_PROMOS_MANAGER_PROMOS_MANAGER_H_
