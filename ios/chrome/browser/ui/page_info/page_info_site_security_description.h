// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_DESCRIPTION_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_DESCRIPTION_H_

#import <UIKit/UIKit.h>

// Config for the information displayed by the page info Site Security section.
@interface PageInfoSiteSecurityDescription : NSObject

@property(nonatomic, copy) NSString* siteURL;
@property(nonatomic, copy) NSString* status;
@property(nonatomic, copy) NSString* securityStatus;
@property(nonatomic, copy) NSString* message;
@property(nonatomic, strong) UIImage* iconImage;
@property(nonatomic, strong) UIColor* iconBackgroundColor;
@property(nonatomic, assign) BOOL isEmpty;
@property(nonatomic, assign) BOOL secure;
@property(nonatomic, assign) BOOL isPageLoading;

@end

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_SITE_SECURITY_DESCRIPTION_H_
