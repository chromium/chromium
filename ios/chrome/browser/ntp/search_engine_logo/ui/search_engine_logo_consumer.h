// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_UI_SEARCH_ENGINE_LOGO_CONSUMER_H_
#define IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_UI_SEARCH_ENGINE_LOGO_CONSUMER_H_

#import <UIKit/UIKit.h>

@protocol SearchEngineLogoConsumer <NSObject>

// Notifies observer that the display state of the doodle has changed.
- (void)doodleDisplayStateChanged:(BOOL)showingDoodle;

@end

#endif  // IOS_CHROME_BROWSER_NTP_SEARCH_ENGINE_LOGO_UI_SEARCH_ENGINE_LOGO_CONSUMER_H_
