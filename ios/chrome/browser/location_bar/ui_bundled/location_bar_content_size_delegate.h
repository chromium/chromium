// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONTENT_SIZE_DELEGATE_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONTENT_SIZE_DELEGATE_H_

#import <Foundation/Foundation.h>

// Delegate notified when the content of the location bar changed in a way that
// can change the width the location bar needs.
//
// The location bar content is mutated from many places (URL updates, badges,
// the Contextual Panel entrypoint, the Reader mode chip), and those mutations
// are independent from the fullscreen transition that sizes the location bar.
// This protocol is implemented both by the embedder measuring the location bar
// (e.g. the toolbar, which sizes the collapsed glass pill from it) and by the
// intermediate owners inside the location bar hierarchy, which re-publish the
// notification so that a change deep in the hierarchy reaches the embedder.
@protocol LocationBarContentSizeDelegate <NSObject>

// Called when the location bar content changed. Any cached measurement of the
// location bar must be considered stale.
//
// This can be called several times in a row and while a fullscreen transition
// is in flight, so implementations must be cheap. Implementations must not
// synchronously lay out the location bar, as this is called from the middle of
// the location bar's own constraint updates.
- (void)locationBarContentSizeDidChange;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_CONTENT_SIZE_DELEGATE_H_
