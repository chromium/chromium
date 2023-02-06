// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_SESSION_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_SESSION_H_

#import <Foundation/Foundation.h>

#import "ios/web/public/find_in_page/crw_find_session.h"

using ResultCountsForQueries = NSDictionary<NSString*, NSNumber*>;

// Fake Find session for testing purposes.
API_AVAILABLE(ios(16))
@interface CRWFakeFindSession : NSObject <CRWFindSession, NSCopying>

// Determines what `resultCount` to return for given fake queries.
@property(nonatomic, strong) ResultCountsForQueries* resultCountsForQueries;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_SESSION_H_
