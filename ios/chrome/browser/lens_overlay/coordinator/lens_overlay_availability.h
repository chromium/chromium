// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_AVAILABILITY_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_AVAILABILITY_H_

// Returns whether the lens overlay is enabled.
bool IsLensOverlayAvailable();

// Returns whether the lens overlay should open navigation in the same tab
// instead of new tab.
bool IsLensOverlaySameTabNavigationEnabled();

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OVERLAY_AVAILABILITY_H_
