// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_HOME_SURFACE_EGTEST_UTILS_H_
#define IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_HOME_SURFACE_EGTEST_UTILS_H_

#import <Foundation/Foundation.h>

// Allows for the Home Surface to open immediately upon backgrounding and
// reopening the app.
void MakeHomeSurfaceOpenImmediately();

// Resets the Home Surface to open after background duration.
void ResetMakeHomeSurfaceOpenImmediately();

#endif  // IOS_CHROME_BROWSER_START_SURFACE_UI_BUNDLED_HOME_SURFACE_EGTEST_UTILS_H_
