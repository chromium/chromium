// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_FIND_IN_PAGE_CONTROLLER_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_FIND_IN_PAGE_CONTROLLER_INTERNAL_H_

#import "ios/web_view/public/cwv_find_in_page_controller.h"

namespace web {
class WebState;
}  // namespace web

NS_ASSUME_NONNULL_BEGIN

@interface CWVFindInPageController ()

- (instancetype)initWithWebState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_FIND_IN_PAGE_CONTROLLER_INTERNAL_H_
