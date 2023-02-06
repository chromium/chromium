// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_INTERACTION_H_
#define IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_INTERACTION_H_

#import "ios/web/public/find_in_page/crw_find_interaction.h"

// Fake Find interaction for testing purposes.
API_AVAILABLE(ios(16))
@interface CRWFakeFindInteraction : NSObject <CRWFindInteraction>

// The active Find session can be set directly instead of being readonly.
@property(nonatomic, strong) id<CRWFindSession> activeFindSession;

@end

#endif  // IOS_WEB_PUBLIC_TEST_FAKES_CRW_FAKE_FIND_INTERACTION_H_
