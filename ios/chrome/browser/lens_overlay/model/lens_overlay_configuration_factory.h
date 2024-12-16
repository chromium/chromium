// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_CONFIGURATION_FACTORY_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_CONFIGURATION_FACTORY_H_

#import <Foundation/Foundation.h>

@class LensConfiguration;
enum class LensOverlayEntrypoint;
enum class LensEntrypoint;
class ProfileIOS;

// A factory class to create configuration objects.
@interface LensOverlayConfigurationFactory : NSObject

// Creates a configuration object for the given entrypoint and profile.
- (LensConfiguration*)configurationForEntrypoint:
                          (LensOverlayEntrypoint)entrypoint
                                         profile:(ProfileIOS*)profile;

// Creates a configuration object for the given entrypoint and profile.
- (LensConfiguration*)configurationForLensEntrypoint:(LensEntrypoint)entrypoint
                                             profile:(ProfileIOS*)profile;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_CONFIGURATION_FACTORY_H_
