// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/infobars/overlays/browser_agent/interaction_handlers/test/mock_ios_add_to_reading_list_infobar_delegate.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

MockIOSAddToReadingListInfobarDelegate::MockIOSAddToReadingListInfobarDelegate(
    ReadingListModel* model,
    web::WebState* web_state)
    : IOSAddToReadingListInfobarDelegate(GURL("http://www.test.com"),
                                         std::u16string(),
                                         0,
                                         model,
                                         web_state) {}

MockIOSAddToReadingListInfobarDelegate::
    ~MockIOSAddToReadingListInfobarDelegate() = default;
