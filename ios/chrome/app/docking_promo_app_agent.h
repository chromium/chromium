// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_DOCKING_PROMO_APP_AGENT_H_
#define IOS_CHROME_APP_DOCKING_PROMO_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/app_state_agent.h"

// App agent that displays the Docking Promo when needed.
@interface DockingPromoAppAgent : NSObject <AppStateAgent>
@end

#endif  // IOS_CHROME_APP_DOCKING_PROMO_APP_AGENT_H_
