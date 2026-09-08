// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_MEDIATOR_H_
#define IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_MEDIATOR_H_

#import <Foundation/Foundation.h>

class HostContentSettingsMap;
@protocol SiteSettingsConsumer;

// Mediator for the Site Settings root screen. Reads default settings from
// `HostContentSettingsMap`, observes changes, and updates the consumer.
@interface SiteSettingsMediator : NSObject

// The consumer receiving setting updates. Setting the consumer triggers an
// initial load.
@property(nonatomic, weak) id<SiteSettingsConsumer> consumer;

// Designated initializer.
- (instancetype)initWithHostContentSettingsMap:
    (HostContentSettingsMap*)settingsMap NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Disconnects observations and clears references.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_SETTINGS_SITE_SETTINGS_COORDINATOR_SITE_SETTINGS_MEDIATOR_H_
