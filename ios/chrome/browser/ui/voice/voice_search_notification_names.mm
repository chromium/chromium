// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/voice/voice_search_notification_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSString* const kVoiceSearchWillShowNotification =
    @"kVoiceSearchWillShowNotification";
NSString* const kVoiceSearchWillHideNotification =
    @"kVoiceSearchWillHideNotification";

NSString* const kTTSAudioReadyForPlaybackNotification =
    @"kTTSAudioReadyForPlaybackNotification";
NSString* const kTTSWillStartPlayingNotification =
    @"kTTSWillStartPlayingNotification";
NSString* const kTTSDidStopPlayingNotification =
    @"kTTSDidStopPlayingNotification";
