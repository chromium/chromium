// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OPEN_LENS_INPUT_SELECTION_COMMAND_H_
#define IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OPEN_LENS_INPUT_SELECTION_COMMAND_H_

#import <UIKit/UIKit.h>
#import "base/ios/block_types.h"

enum class LensEntrypoint;

// Enum representing the possible ways the input selection UI can be presented.
enum class LensInputSelectionPresentationStyle {
  SlideFromRight = 0,
  SlideFromLeft = 1,
};

// An instance of this class contains the data needed to do a Lens search via
// the input selection UI AKA live view finder.
@interface OpenLensInputSelectionCommand : NSObject

// Initializes to open the Lens input election UI with `entryPoint`.
- (instancetype)initWithEntryPoint:(LensEntrypoint)entryPoint
                 presentationStyle:
                     (LensInputSelectionPresentationStyle)presentationStyle
            presentationCompletion:(ProceduralBlock)presentationCompletion
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The entry point to pass to Lens.
@property(nonatomic, assign, readonly) LensEntrypoint entryPoint;

// The presentation style.
@property(nonatomic, assign, readonly)
    LensInputSelectionPresentationStyle presentationStyle;

// The completion block to be run on the main thread.
@property(nonatomic, strong, readonly) ProceduralBlock presentationCompletion;

// If set to YES, an IPH bubble will be presented on the NTP that points to the
// Lens icon in the NTP fakebox, if Lens is dismissed by the user.
@property(nonatomic, assign) BOOL presentNTPLensIconBubbleOnDismiss;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_PUBLIC_COMMANDS_OPEN_LENS_INPUT_SELECTION_COMMAND_H_
