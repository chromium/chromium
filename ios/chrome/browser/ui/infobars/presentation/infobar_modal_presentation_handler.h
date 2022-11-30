// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_HANDLER_H_
#define IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_HANDLER_H_

#import <Foundation/Foundation.h>

// Handler for the infobar modal presentation.
@protocol InfobarModalPresentationHandler <NSObject>

// The content inside the modal has been updated and the frame needs to be
// resized.
- (void)resizeInfobarModal;

@end

#endif  // IOS_CHROME_BROWSER_UI_INFOBARS_PRESENTATION_INFOBAR_MODAL_PRESENTATION_HANDLER_H_
