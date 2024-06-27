// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SNAPSHOT_CONSUMER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SNAPSHOT_CONSUMER_H_

@class UIImage;

/// Consumer for lens overlay snapshots (screenshots).
@protocol LensOverlaySnapshotConsumer

// Loads the captured snapshot.
- (void)loadSnapshot:(UIImage*)snapshot;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_SNAPSHOT_CONSUMER_H_
