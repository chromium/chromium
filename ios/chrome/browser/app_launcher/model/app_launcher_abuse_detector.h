// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_ABUSE_DETECTOR_H_
#define IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_ABUSE_DETECTOR_H_

#import <Foundation/Foundation.h>

class GURL;

typedef NS_ENUM(NSInteger, ExternalAppLaunchPolicy) {
  // Allow the application to launch.
  ExternalAppLaunchPolicyAllow = 0,
  // Prompt the user with a dialog, so they can choose wether to block the
  // application or launch it.
  ExternalAppLaunchPolicyPrompt,
  // Block launching the application for this session.
  // TODO(crbug.com/41473755): Remove this policy once non-modal dialogs are
  // used
  // by default.
  ExternalAppLaunchPolicyBlock,
};

// The maximum allowed number of consecutive launches of the same app before
// starting to prompt.
extern const int kMaxAllowedConsecutiveExternalAppLaunches;

// This class uses a maps between external application redirection keys and
// external app launching state, so it can unwanted behaviors of repeated
// application launching.
// Each key is a space separated combination of the absloute string for the
// original source URL, and the scheme of the external Application URL.
@interface AppLauncherAbuseDetector : NSObject

// Updates the state for external application `URL` and `sourcePageURL` with a
// new app launch request.
- (void)didRequestLaunchExternalAppURL:(const GURL&)URL
                     fromSourcePageURL:(const GURL&)sourcePageURL;

// Returns the launching recommendation based on the current state for given
// application `URL` and `sourcePageURL`.
- (ExternalAppLaunchPolicy)launchPolicyForURL:(const GURL&)URL
                            fromSourcePageURL:(const GURL&)sourcePageURL;

// Sets the state of the application `URL` and `sourcePageURL` as blocked for
// the remaining lifetime of this object.
- (void)blockLaunchingAppURL:(const GURL&)URL
           fromSourcePageURL:(const GURL&)sourcePageURL;
@end

#endif  // IOS_CHROME_BROWSER_APP_LAUNCHER_MODEL_APP_LAUNCHER_ABUSE_DETECTOR_H_
