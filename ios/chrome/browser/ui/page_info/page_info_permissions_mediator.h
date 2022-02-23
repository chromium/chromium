// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PERMISSIONS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PERMISSIONS_MEDIATOR_H_

#import "ios/chrome/browser/ui/page_info/page_info_view_controller_permissions_delegate.h"

namespace web {
class WebState;
}

// Mediator for the page info permissions.
API_AVAILABLE(ios(15.0))
@interface PageInfoPermissionsMediator
    : NSObject <PageInfoViewControllerPermissionsDelegate>

- (instancetype)init NS_UNAVAILABLE;

// Designated initializer that reads information from |webState| to establish
// the property.
- (instancetype)initWithWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_PERMISSIONS_MEDIATOR_H_
