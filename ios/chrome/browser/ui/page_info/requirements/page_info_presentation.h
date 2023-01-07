// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_REQUIREMENTS_PAGE_INFO_PRESENTATION_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_REQUIREMENTS_PAGE_INFO_PRESENTATION_H_

#import <CoreGraphics/CoreGraphics.h>

@class UIView;

// PageInfoPresentation contains methods related to the presentation of the Page
// Info UI.
@protocol PageInfoPresentation

// Presents the `pageInfoView` for the Page Info UI. Implementors must ensure
// that `pageInfoView` is the appropriate size for presentation.
- (void)presentPageInfoView:(UIView*)pageInfoView;

// Called before the Page Info UI is presented.
- (void)prepareForPageInfoPresentation;

// Converts `origin` to the coordinate system used for presenting the Page Info
// UI. `origin` should be in window coordinates.
- (CGPoint)convertToPresentationCoordinatesForOrigin:(CGPoint)origin;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_REQUIREMENTS_PAGE_INFO_PRESENTATION_H_
