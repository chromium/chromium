// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_AUTOFILL_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_AUTOFILL_H_

#import "cwv_web_view.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillController;

@interface CWVWebView (Autofill)

// The web view's autofill controller.
@property(nonatomic, readonly) CWVAutofillController* autofillController;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_WEB_VIEW_AUTOFILL_H_
