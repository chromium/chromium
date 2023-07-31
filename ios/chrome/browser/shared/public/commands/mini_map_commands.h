// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MINI_MAP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MINI_MAP_COMMANDS_H_

namespace web {
class WebState;
}

// Commands related to the Mini Map feature.
@protocol MiniMapCommands <NSObject>

// Shows the interstitial consent if needed then present the minimap for text.
- (void)presentConsentThenMiniMapForText:(NSString*)text
                              inWebState:(web::WebState*)webState;

// Shows the minimap for text.
- (void)presentMiniMapForText:(NSString*)text
                   inWebState:(web::WebState*)webState;

// Shows the minimap directions for text.
- (void)presentMiniMapDirectionsForText:(NSString*)text
                             inWebState:(web::WebState*)webState;

// Hides the minimap.
- (void)hideMiniMap;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MINI_MAP_COMMANDS_H_
