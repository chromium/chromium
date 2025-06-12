// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MINI_MAP_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MINI_MAP_COMMANDS_H_

// Commands related to the Mini Map feature.
@protocol MiniMapCommands <NSObject>

// Shows the minimap for `text`.
- (void)presentMiniMapForText:(NSString*)text;

// Shows the minimap for `text`.
- (void)presentMiniMapWithIPHForText:(NSString*)text;

// Shows the minimap directions for text.
- (void)presentMiniMapDirectionsForText:(NSString*)text;

// Shows the minimap for `URL`.
- (void)presentMiniMapForURL:(NSURL*)URL;

// Hides the minimap.
- (void)hideMiniMap;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_MINI_MAP_COMMANDS_H_
