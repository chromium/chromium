// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
#define IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/1012697): Remove this protocol when SceneController is
// operational. Move the private internals back into MainController, and pass
// ownership of Scene-related objects to SceneController.
@protocol MainControllerGuts

// MainController tracks EULA acceptance and performs delayed tasks when the
// first run UI is dismissed.
- (void)prepareForFirstRunUI:(SceneState*)presentingScene;

@end

#endif  // IOS_CHROME_APP_MAIN_CONTROLLER_GUTS_H_
