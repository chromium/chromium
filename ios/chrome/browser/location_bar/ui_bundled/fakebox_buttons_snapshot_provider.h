// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_FAKEBOX_BUTTONS_SNAPSHOT_PROVIDER_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_FAKEBOX_BUTTONS_SNAPSHOT_PROVIDER_H_

// Provides a snapshot of the fakebox buttons that can be overlaid on the
// LocationBar and faded out during the focus transition, and faded in during
// the defocus transition.
@protocol FakeboxButtonsSnapshotProvider

// Returns a view that will used during focus and defocus transitions.
- (UIView*)fakeboxButtonsSnapshot;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_FAKEBOX_BUTTONS_SNAPSHOT_PROVIDER_H_
