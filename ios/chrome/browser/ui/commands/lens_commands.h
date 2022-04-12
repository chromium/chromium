// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_COMMANDS_LENS_COMMANDS_H_
#define IOS_CHROME_BROWSER_UI_COMMANDS_LENS_COMMANDS_H_

@class SearchImageWithLensCommand;

// Commands related to Lens.
@protocol LensCommands

// Search for an image with Lens, using |command| parameters.
- (void)searchImageWithLens:(SearchImageWithLensCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_UI_COMMANDS_LENS_COMMANDS_H_
