// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_BADGE_UI_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_BADGE_UI_DELEGATE_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}  // namespace web

enum class InfobarType;

// Delegate that handles any followup actions to Infobar UI events.
@protocol InfobarBadgeUIDelegate

// Called whenever the accept/confirm button of an Infobar of type |infobarType|
// in |webState| was tapped. It is triggered by either the banner or modal
// button. |webState| cannot be nil.
- (void)infobarWasAccepted:(InfobarType)infobarType
               forWebState:(web::WebState*)webState;

// Called whenever the revert action button of an Infobar of type |infobarType|
// in |webState| was tapped (e.g. the Translate "Show Original" action). It is
// triggered by the banner button. |webState| cannot be nil.
- (void)infobarWasReverted:(InfobarType)infobarType
               forWebState:(web::WebState*)webState;

// Called whenever an InfobarBanner of type |infobarType| in |webState| was
// presented. |webState| cannot be nil.
- (void)infobarBannerWasPresented:(InfobarType)infobarType
                      forWebState:(web::WebState*)webState;

// Called whenever an InfobarBanner of type |infobarType| in |webState| was
// dismissed. |webState| cannot be nil.
- (void)infobarBannerWasDismissed:(InfobarType)infobarType
                      forWebState:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_INFOBAR_BADGE_UI_DELEGATE_H_
