// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_H_
#define IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"

class Browser;
class GURL;

// This manager maintains all state related to sharing the active URL to other
// devices. It has the role of a dispatcher that shares the active URL to
// various internal sharing services (e.g. handoff).
class DeviceSharingManager : public KeyedService {
 public:
  DeviceSharingManager() = default;

  // Set `browser` as the active browser. It will remain the active browser
  // until another active browser is set.
  virtual void SetActiveBrowser(Browser* browser) = 0;

  // If `browser` is the active browser, set `active_url` as the active URL.
  // If `browser` is not the active browser, do nothing.
  virtual void UpdateActiveUrl(Browser* browser, const GURL& active_url) = 0;

  // If `browser` is the active browser, set `active_title` as the active
  // page title. If `browser` is not the active browser, do nothing.
  virtual void UpdateActiveTitle(Browser* browser,
                                 const std::u16string& active_title) = 0;

  // If `browser` is the active browser, clear the active URL and title.
  // If `browser` is not the active browser, do nothing.
  virtual void ClearActiveUrl(Browser* browser) = 0;
};

#endif  // IOS_CHROME_BROWSER_DEVICE_SHARING_MODEL_DEVICE_SHARING_MANAGER_H_
