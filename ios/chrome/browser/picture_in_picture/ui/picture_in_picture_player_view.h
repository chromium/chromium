// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_PLAYER_VIEW_H_
#define IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_PLAYER_VIEW_H_

#import <UIKit/UIKit.h>

@class AVPlayerLayer;

// A UIView subclass that provides an AVPlayerLayer as its backing layer.
@interface PictureInPicturePlayerView : UIView

// The player layer that backs this view.
@property(nonatomic, readonly) AVPlayerLayer* playerLayer;

@end

#endif  // IOS_CHROME_BROWSER_PICTURE_IN_PICTURE_UI_PICTURE_IN_PICTURE_PLAYER_VIEW_H_
