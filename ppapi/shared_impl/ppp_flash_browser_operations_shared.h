// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PPP_FLASH_BROWSER_OPERATIONS_SHARED_H_
#define PPAPI_SHARED_IMPL_PPP_FLASH_BROWSER_OPERATIONS_SHARED_H_

#include <string>
#include <vector>

#include "ppapi/c/private/ppp_flash_browser_operations.h"

namespace ppapi {

struct FlashSiteSetting {
  FlashSiteSetting()
      : permission(PP_FLASH_BROWSEROPERATIONS_PERMISSION_DEFAULT) {}
  FlashSiteSetting(const std::string& in_site,
                   PP_Flash_BrowserOperations_Permission in_permission)
      : site(in_site), permission(in_permission) {}

  std::string site;
  PP_Flash_BrowserOperations_Permission permission;
};

typedef std::vector<FlashSiteSetting> FlashSiteSettings;

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PPP_FLASH_BROWSER_OPERATIONS_SHARED_H_
