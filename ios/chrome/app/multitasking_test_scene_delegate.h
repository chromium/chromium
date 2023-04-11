// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MULTITASKING_TEST_SCENE_DELEGATE_H_
#define IOS_CHROME_APP_MULTITASKING_TEST_SCENE_DELEGATE_H_

#import "ios/chrome/browser/shared/coordinator/scene/scene_delegate.h"

// Scene delegate that overrides the main scene's window in multitasking tests
// to be smaller.
@interface MultitaskingTestSceneDelegate : SceneDelegate
@end

#endif  // IOS_CHROME_APP_MULTITASKING_TEST_SCENE_DELEGATE_H_
