// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_DELEGATE_H_

#import "components/lens/lens_overlay_new_tab_source.h"

@class LensResultPageMediator;
class GURL;
namespace web {
class WebState;
}  // namespace web

/// Delegate for the lens result page mediator.
@protocol LensResultPageMediatorDelegate

/// Called when the web state gets destroyed.
- (void)lensResultPageWebStateDestroyed;

/// Called when the active `webState` in LensResultPageMediator changes.
- (void)lensResultPageDidChangeActiveWebState:(web::WebState*)webState;

/// Called when a new tab has been opened by the lens result page mediator.
- (void)lensResultPageMediator:(LensResultPageMediator*)mediator
       didOpenNewTabFromSource:(lens::LensOverlayNewTabSource)newTabSource;

/// Called when an URL needs to be opened in a new tab.
- (void)lensResultPageOpenURLInNewTabRequsted:(GURL)URL;

/// Called when a URL that is not an LRP is loaded in the bottom sheet.
/// This can happen in cases like user pressing on a "related search" chip.
- (void)lensResultPageWillLoadNonLensSRP:(NSString*)queryText
                                     url:(const GURL&)destinationURL;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_RESULT_PAGE_MEDIATOR_DELEGATE_H_
