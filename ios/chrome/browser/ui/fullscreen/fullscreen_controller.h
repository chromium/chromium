// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_

#import <UIKit/UIKit.h>

#include "base/supports_user_data.h"

class Browser;
@class ChromeBroadcaster;
class FullscreenControllerObserver;

// An object that observes scrolling events in the main content area and
// calculates how much of the toolbar should be visible as a result.  When the
// user scrolls down the screen, the toolbar should be hidden to allow more of
// the page's content to be visible.
class FullscreenController : public base::SupportsUserData::Data {
 public:
  explicit FullscreenController() = default;

  // Retrieves the FullscreenController for `browser`. This should only be
  // called with the kFullscreenControllerBrowserScoped turned on.
  static FullscreenController* FromBrowser(Browser* browser);

  // The ChromeBroadcaster through the FullscreenController receives UI
  // information necessary to calculate fullscreen progress.
  // TODO(crbug.com/41358770): Once FullscreenController is a BrowserUserData,
  // remove this ad-hoc broadcaster and drive the animations via the Browser's
  // ChromeBroadcaster.
  virtual ChromeBroadcaster* broadcaster() = 0;

  // Adds and removes FullscreenControllerObservers.
  virtual void AddObserver(FullscreenControllerObserver* observer) = 0;
  virtual void RemoveObserver(FullscreenControllerObserver* observer) = 0;

  // FullscreenController can be disabled when a feature requires that the
  // toolbar be fully visible.  Since there are multiple reasons fullscreen
  // might need to be disabled, this state is represented by a counter rather
  // than a single bool.  When a feature needs the toolbar to be visible, it
  // calls IncrementDisabledCounter().  After that feature no longer requires
  // the toolbar, it calls DecrementDisabledCounter().  IsEnabled() returns
  // true when the counter is equal to zero.  ScopedFullscreenDisabler can be
  // used to tie a disabled counter to an object's lifetime.
  virtual bool IsEnabled() const = 0;
  virtual void IncrementDisabledCounter() = 0;
  virtual void DecrementDisabledCounter() = 0;

  // Returns whether fullscreen is implemented by resizing the web view scroll
  // view rather than setting the content inset.
  virtual bool ResizesScrollView() const = 0;

  // FullscreenController isn't notified when the trait collection of the
  // browser is changed. This method is here to notify it.
  virtual void BrowserTraitCollectionChangedBegin() = 0;
  virtual void BrowserTraitCollectionChangedEnd() = 0;

  // Returns the current fullscreen progress value.  This is a float between 0.0
  // and 1.0, where 0.0 denotes that the toolbar should be completely hidden and
  // 1.0 denotes that the toolbar should be completely visible.
  virtual CGFloat GetProgress() const = 0;

  // Returns the max and min insets for the visible content area's viewport.
  // The max insets correspond to a progress of 1.0, and the min insets are for
  // progress 0.0.
  virtual UIEdgeInsets GetMinViewportInsets() const = 0;
  virtual UIEdgeInsets GetMaxViewportInsets() const = 0;

  // Returns the current insets for the visible content area's viewport.
  virtual UIEdgeInsets GetCurrentViewportInsets() const = 0;

  // Enters fullscreen mode, animating away toolbars and resetting the progress
  // to 0.0.  Calling this function while fullscreen is disabled has no effect.
  virtual void EnterFullscreen() = 0;

  // Exits fullscreen mode, animating in toolbars and resetting the progress to
  // 1.0.
  virtual void ExitFullscreen() = 0;

  // Exits fullscreen without animation, resetting the progress to 1.0.
  virtual void ExitFullscreenWithoutAnimation() = 0;

  // Force fullscreen mode is used when the bottom omnibox is collapsed above
  // the keyboard. When the mode is active:
  // - Fullscreen progress is forced to 0 and should stay at 0.
  // - Updating browser insets through fullscreen are disabled. (crbug/1490601)
  // When exiting the mode, fullscreen is reset.
  virtual bool IsForceFullscreenMode() const = 0;
  virtual void EnterForceFullscreenMode() = 0;
  virtual void ExitForceFullscreenMode() = 0;

  // Force horizontal content resize, when content isn't tracking resize by
  // itself.
  virtual void ResizeHorizontalViewport() = 0;

 protected:
  // Returns the key used to store the UserData. Protected so it can be used in
  // tests.
  static const void* UserDataKey();
};

#endif  // IOS_CHROME_BROWSER_UI_FULLSCREEN_FULLSCREEN_CONTROLLER_H_
