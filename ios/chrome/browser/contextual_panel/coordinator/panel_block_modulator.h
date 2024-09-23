// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_BLOCK_MODULATOR_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_BLOCK_MODULATOR_H_

#import <UIKit/UIKit.h>

#import "base/memory/weak_ptr.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_block_data.h"

class Browser;
struct ContextualPanelItemConfiguration;

// A coordinator-like object that manages a collection view cell and other
// modulators. The major difference between this and a coordinator is that it
// does not own or present its own UI. These modulators display Contextual Panel
// Info Blocks.
@interface PanelBlockModulator : NSObject

// Creates a modulator that uses `viewController` and `browser` with the given
// `itemConfiguration` as initial data.
- (instancetype)initWithBaseViewController:(UIViewController*)viewController
                                   browser:(Browser*)browser
                         itemConfiguration:
                             (base::WeakPtr<ContextualPanelItemConfiguration>)
                                 itemConfiguration NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// The view controller this modulator was initialized with.
@property(weak, nonatomic, readonly) UIViewController* baseViewController;

// The modulator's Browser, if one was assigned.
@property(assign, nonatomic, readonly) Browser* browser;

// The item configuration this modulator was initialized with.
@property(assign, nonatomic, readonly)
    base::WeakPtr<ContextualPanelItemConfiguration>
        itemConfiguration;

// The data the UI can use to render this panel block.
// Subclasses should override this to return the correct data for their block.
- (PanelBlockData*)panelBlockData;

// The type of this block.
- (NSString*)blockType;

// The basic lifecycle methods for modulators are -start and -stop. These
// are blank template methods; child classes are expected to implement them and
// do not need to invoke the superclass methods. Subclasses of
// PanelBlockModulator that expect to be subclassed should not build
// functionality into these methods. Starts the user interaction managed by the
// receiver.
- (void)start;

// Stops the user interaction managed by the receiver.
- (void)stop;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_COORDINATOR_PANEL_BLOCK_MODULATOR_H_
