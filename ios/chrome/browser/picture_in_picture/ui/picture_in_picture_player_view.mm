// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/picture_in_picture/ui/picture_in_picture_player_view.h"

#import <AVFoundation/AVFoundation.h>

#import "base/apple/foundation_util.h"

@implementation PictureInPicturePlayerView

+ (Class)layerClass {
  return [AVPlayerLayer class];
}

- (AVPlayerLayer*)playerLayer {
  return base::apple::ObjCCastStrict<AVPlayerLayer>(self.layer);
}

@end
