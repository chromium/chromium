// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_DATA_MANAGER_OBSERVER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_DATA_MANAGER_OBSERVER_H_

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class CWVAutofillDataManager;

// Protocol to receive change notifications from CWVAutofillDataManager.
@protocol CWVAutofillDataManagerObserver<NSObject>

// Called whenever CWVAutofillDataManager's autofill profiles or credit cards
// have been loaded for the first time, added, deleted, or updated.
- (void)autofillDataManagerDataDidChange:
    (CWVAutofillDataManager*)autofillDataManager;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_AUTOFILL_DATA_MANAGER_OBSERVER_H_
