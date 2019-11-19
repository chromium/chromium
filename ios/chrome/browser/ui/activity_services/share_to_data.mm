// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/activity_services/share_to_data.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface ShareToData () {
  // URL to be shared with share extensions.
  GURL _shareURL;

  // Visible URL of the page.
  GURL _visibleURL;
}
@end

@implementation ShareToData

@synthesize title = _title;
@synthesize thumbnailGenerator = _thumbnailGenerator;
@synthesize isOriginalTitle = _isOriginalTitle;
@synthesize isPagePrintable = _isPagePrintable;
@synthesize isPageSearchable = _isPageSearchable;
@synthesize userAgent = _userAgent;

- (id)initWithShareURL:(const GURL&)shareURL
            visibleURL:(const GURL&)visibleURL
                 title:(NSString*)title
       isOriginalTitle:(BOOL)isOriginalTitle
       isPagePrintable:(BOOL)isPagePrintable
      isPageSearchable:(BOOL)isPageSearchable
             userAgent:(web::UserAgentType)userAgent
    thumbnailGenerator:
        (ChromeActivityItemThumbnailGenerator*)thumbnailGenerator {
  DCHECK(shareURL.is_valid());
  DCHECK(visibleURL.is_valid());
  DCHECK(title);
  self = [super init];
  if (self) {
    _shareURL = shareURL;
    _visibleURL = visibleURL;
    _title = [title copy];
    _isOriginalTitle = isOriginalTitle;
    _isPagePrintable = isPagePrintable;
    _isPageSearchable = isPageSearchable;
    _userAgent = userAgent;
    _thumbnailGenerator = thumbnailGenerator;
  }
  return self;
}

- (const GURL&)shareURL {
  return _shareURL;
}

- (const GURL&)visibleURL {
  return _visibleURL;
}

- (NSURL*)shareNSURL {
  return net::NSURLWithGURL(_shareURL);
}

- (NSURL*)passwordManagerNSURL {
  return net::NSURLWithGURL(_visibleURL);
}

@end
