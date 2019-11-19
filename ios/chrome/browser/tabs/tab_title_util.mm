// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/tab_title_util.h"

#import <Foundation/Foundation.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/download_manager_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace tab_util {

NSString* GetTabTitle(web::WebState* web_state) {
  base::string16 title;
  web::NavigationManager* navigationManager = web_state->GetNavigationManager();
  DownloadManagerTabHelper* downloadTabHelper =
      DownloadManagerTabHelper::FromWebState(web_state);
  if (navigationManager && downloadTabHelper &&
      !navigationManager->GetVisibleItem() &&
      downloadTabHelper->has_download_task()) {
    title = l10n_util::GetStringUTF16(IDS_DOWNLOAD_TAB_TITLE);
  } else {
    title = web_state->GetTitle();
    if (title.empty())
      title = l10n_util::GetStringUTF16(IDS_DEFAULT_TAB_TITLE);
  }
  return base::SysUTF16ToNSString(title);
}

}  // namespace tab_util
