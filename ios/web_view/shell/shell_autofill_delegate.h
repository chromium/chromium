// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_SHELL_SHELL_AUTOFILL_DELEGATE_H_
#define IOS_WEB_VIEW_SHELL_SHELL_AUTOFILL_DELEGATE_H_

#import <Foundation/Foundation.h>
#import "ios/web_view/shell/cwv_framework.h"

NS_ASSUME_NONNULL_BEGIN

@interface ShellAutofillDelegate : NSObject<CWVAutofillControllerDelegate>
@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_SHELL_SHELL_AUTOFILL_DELEGATE_H_
