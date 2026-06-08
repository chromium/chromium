// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/toolbar/legacy/ui_bundled/adaptive_toolbar_app_interface.h"

#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/ui_bundled/test_infobar_delegate.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"

@implementation AdaptiveToolbarAppInterface

+ (BOOL)addInfobarWithTitle:(NSString*)title {
  infobars::InfoBarManager* manager =
      InfoBarManagerImpl::FromWebState(chrome_test_util::GetCurrentWebState());
  TestInfoBarDelegate* test_infobar_delegate = new TestInfoBarDelegate(title);
  return test_infobar_delegate->Create(manager);
}

@end
