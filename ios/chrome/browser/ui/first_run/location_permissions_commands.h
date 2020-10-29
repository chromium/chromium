// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_COMMANDS_H_

// Command to signal to receiver to handle location permissions modal actions.
@protocol LocationPermissionsCommands <NSObject>

// Command the modal to be hidden.
- (void)dismissLocationPermissionsExplanationModal;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_LOCATION_PERMISSIONS_COMMANDS_H_
