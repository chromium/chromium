// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/activity_services/data/share_to_data.h"

#import "base/check.h"
#import "base/strings/sys_string_conversions.h"
#import "net/base/apple/url_conversions.h"

@interface ShareToData () {
  // URL to be shared with share extensions.
  GURL _shareURL;

  // Visible URL of the page.
  GURL _visibleURL;
}
@end

@implementation ShareToData

- (id)initWithShareURL:(const GURL&)shareURL
            visibleURL:(const GURL&)visibleURL
                 title:(NSString*)title
        additionalText:(NSString*)additionalText
       isOriginalTitle:(BOOL)isOriginalTitle
       isPagePrintable:(BOOL)isPagePrintable
      isPageSearchable:(BOOL)isPageSearchable
      canSendTabToSelf:(BOOL)canSendTabToSelf
             userAgent:(web::UserAgentType)userAgent
    thumbnailGenerator:(ChromeActivityItemThumbnailGenerator*)thumbnailGenerator
          linkMetadata:(LPLinkMetadata*)linkMetadata {
  DCHECK(shareURL.is_valid());
  DCHECK(visibleURL.is_valid());
  DCHECK(title);
  self = [super init];
  if (self) {
    _shareURL = shareURL;
    _visibleURL = visibleURL;
    _title = [title copy];
    _additionalText = [additionalText copy];
    _isOriginalTitle = isOriginalTitle;
    _isPagePrintable = isPagePrintable;
    _isPageSearchable = isPageSearchable;
    _canSendTabToSelf = canSendTabToSelf;
    _userAgent = userAgent;
    _thumbnailGenerator = thumbnailGenerator;
    _linkMetadata = linkMetadata;
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

@end
