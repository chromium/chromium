// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_KEYS_HELPER_H_
#define IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_KEYS_HELPER_H_

@class NSString;
@class NSArray;

namespace crash_keys {

// Sets a key if `background` is true, unset if false. This will allow tracking
// of crashes that occur when the app is backgrounded.
void SetCurrentlyInBackground(bool background);

// Sets a key if the app is terminating. This will allow tracking of crashes
// that occur when the app is terminating.
void SetCrashedAfterAppWillTerminate();

// Sets a key if `signedIn` is true, unset if false. The key indicates that the
// user is signed-in.
void SetCurrentlySignedIn(bool signedIn);

// Sets a key to indicate the number of memory warnings the application has
// received over its lifetime, or unset the key if the count is zero.
void SetMemoryWarningCount(int count);

// Sets a key indicating a memory warning is deemed to be in progress (if value
// is 'true'), otherwise remove the key.
void SetMemoryWarningInProgress(bool value);

// Sets a key indicating the current free memory amount in KB. 0 does not remove
// the key as getting no memory is important information.
void SetCurrentFreeMemoryInKB(int value);

// Increases a key indicating the number of PDF tabs opened. If value is TRUE,
// the counter is increased. If value is FALSE, the counter is decreased. If
// counter falls to 0, the entry is removed. This function does not keep
// previous state. If SetCurrentTabIsPDF is called twice with TRUE, the counter
// will be incremented twice.
void SetCurrentTabIsPDF(bool value);

// Sets a key in profile dictionary to store the device orientation.
// Each values is 1 digit: first is the UI orientation from the Foundation
// UIInterfaceOrientation enum (values decimal from 1 to 4) and the second is
// the device orientation with values from the Foundation UIDeviceOrientation
// enum (values decimal from 0 to 7).
void SetCurrentOrientation(int statusBarOrientation, int deviceOrientation);

// Sets a key in profile dictionary to store the device horizontal size
// class. The values are from the UIKit UIUserInterfaceSizeClass enum (decimal
// values from 0 to 2).
void SetCurrentHorizontalSizeClass(int horizontalSizeClass);

// Sets a key in profile dictionary to store the device user interface
// style. The values are from the UIKit UIUserInterfaceStyle enum (decimal
// values from 0 to 2).
void SetCurrentUserInterfaceStyle(int userInterfaceStyle);

// Sets the number of connected scenes. Only reported if not 1.
void SetConnectedScenesCount(int connectedScenes);

// Sets the number of foreground scenes. Only reported if not 1.
void SetForegroundScenesCount(int connectedScenes);

// Sets a key in profile dictionary to store the count of regular tabs.
void SetRegularTabCount(int tabCount);

// Sets a key in profile dictionary to store the count of inactive tabs.
void SetInactiveTabCount(int tabCount);

// Sets a key in profile dictionary to store the count of incognito tabs.
void SetIncognitoTabCount(int tabCount);

// Sets a key indicating that destroying and rebuilding the incognito browser
// state is in progress, otherwise remove the key.
void SetDestroyingAndRebuildingIncognitoBrowserState(bool in_progress);

// Sets a key to help debug a crash when animating from grid to visible tab.
// `to_view_controller` is the view controller about to be presented. The
// remaining parameters relate to the `to_view_controller`.
void SetGridToVisibleTabAnimation(NSString* to_view_controller,
                                  NSString* presenting_view_controller,
                                  NSString* presented_view_controller,
                                  NSString* parent_view_controller);

// Removes the key to help debug a crash when animating from grid to visible
// tab.
void RemoveGridToVisibleTabAnimation();

// Sets a key in browser to store the playback state of media player (audio or
// video). This function records a new start. This function is called for each
// stream in the media (once or twice for audio, two or three times for video).
void MediaStreamPlaybackDidStart();

// Sets a key in browser to store the playback state of media player (audio or
// video). This function records a stop or pause. This function must be called
// the same number of times as MediaStreamPlaybackDidStart.
void MediaStreamPlaybackDidStop();

// Sets whether VoiceOver is currently running or not.
void SetVoiceOverRunning(bool running);

}  // namespace crash_keys

#endif  // IOS_CHROME_BROWSER_CRASH_REPORT_MODEL_CRASH_KEYS_HELPER_H_
