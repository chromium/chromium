// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_COMMANDS_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_COMMANDS_H_

@class OpenLensInputSelectionCommand;
@class SearchImageWithLensCommand;
enum class LensEntrypoint;

// Commands related to Lens.
@protocol LensCommands

// Search for an image with Lens, using `command` parameters.
- (void)searchImageWithLens:(SearchImageWithLensCommand*)command;

// Opens the input selection UI with the given settings.
- (void)openLensInputSelection:(OpenLensInputSelectionCommand*)command;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_LENS_COMMANDS_H_
