// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_adaptor.h"

#include "base/logging.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/ui/tab_grid/tab_grid_paging.h"
#import "ios/chrome/browser/ui/tab_grid/view_controller_swapping.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"
#import "ios/web/public/navigation/navigation_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabGridAdaptor
// TabSwitcher properties.
@synthesize delegate = _delegate;

#pragma mark - TabSwitcher

- (id<ApplicationCommands, OmniboxFocuser, ToolbarCommands>)dispatcher {
  return static_cast<id<ApplicationCommands, OmniboxFocuser, ToolbarCommands>>(
      self.adaptedDispatcher);
}

- (void)restoreInternalStateWithMainTabModel:(TabModel*)mainModel
                                 otrTabModel:(TabModel*)otrModel
                              activeTabModel:(TabModel*)activeModel {
  // The only action here is to signal to the tab grid which panel should be
  // active.
  if (activeModel == otrModel) {
    self.tabGridPager.activePage = TabGridPageIncognitoTabs;
  } else {
    self.tabGridPager.activePage = TabGridPageRegularTabs;
  }
}

- (UIViewController*)viewController {
  return self.tabGridViewController;
}

- (void)dismissWithNewTabAnimationToModel:(TabModel*)targetModel
                        withUrlLoadParams:(const UrlLoadParams&)urlLoadParams
                                  atIndex:(NSUInteger)position {
  NSUInteger tabIndex = position;
  if (position > targetModel.count)
    tabIndex = targetModel.count;

  // Create the new tab.
  [targetModel insertWebStateWithLoadParams:urlLoadParams.web_params
                                     opener:nil
                                openedByDOM:NO
                                    atIndex:tabIndex
                               inBackground:NO];

  // Tell the delegate to display the tab.
  DCHECK(self.delegate);
  [self.delegate tabSwitcher:self
      shouldFinishWithActiveModel:targetModel
                     focusOmnibox:NO];
}

- (void)setOtrTabModel:(TabModel*)otrModel {
  DCHECK(self.incognitoMediator);
  self.incognitoMediator.tabModel = otrModel;
}

@end
