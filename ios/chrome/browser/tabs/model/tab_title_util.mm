// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/tabs/model/tab_title_util.h"

#import <Foundation/Foundation.h>

#import "base/strings/sys_string_conversions.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/download/model/download_manager_tab_helper.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/web_state.h"
#import "ui/base/l10n/l10n_util.h"

namespace tab_util {
namespace {

// Returns whether `web_state` only has a download in-progress.
bool WebStateHasDownloadInProgress(const web::WebState* web_state) {
  if (!web_state->IsRealized())
    return false;

  const web::NavigationManager* navigation_manager =
      web_state->GetNavigationManager();
  if (!navigation_manager)
    return false;

  if (navigation_manager->GetVisibleItem())
    return false;

  const DownloadManagerTabHelper* download_tab_helper =
      DownloadManagerTabHelper::FromWebState(web_state);
  if (!download_tab_helper)
    return false;

  return download_tab_helper->has_download_task();
}

}

NSString* GetTabTitle(const web::WebState* web_state) {
  if (WebStateHasDownloadInProgress(web_state)) {
    return l10n_util::GetNSString(IDS_DOWNLOAD_TAB_TITLE);
  }

  const std::u16string& title = web_state->GetTitle();
  if (!title.empty())
    return base::SysUTF16ToNSString(title);

  return l10n_util::GetNSString(IDS_DEFAULT_TAB_TITLE);
}

}  // namespace tab_util
