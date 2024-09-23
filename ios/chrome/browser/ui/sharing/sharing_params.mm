// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/sharing/sharing_params.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"

#import "url/gurl.h"

@implementation SharingParams

- (instancetype)initWithScenario:(SharingScenario)scenario {
  if ((self = [super init])) {
    _scenario = scenario;
  }
  return self;
}

- (instancetype)initWithImage:(UIImage*)image
                        title:(NSString*)title
                     scenario:(SharingScenario)scenario {
  DCHECK(image);
  DCHECK(title);
  if ((self = [self initWithScenario:scenario])) {
    _image = image;
    _imageTitle = title;
  }
  return self;
}

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
                   scenario:(SharingScenario)scenario {
  self = [self initWithURLs:@[ [[URLWithTitle alloc] initWithURL:URL
                                                           title:title] ]
                   scenario:scenario];
  return self;
}

- (instancetype)initWithURLs:(NSArray<URLWithTitle*>*)URLs
                    scenario:(SharingScenario)scenario {
  DCHECK(URLs.count);
  if ((self = [self initWithScenario:scenario])) {
    _URLs = URLs;
  }
  return self;
}

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
             additionalText:(NSString*)additionalText
                   scenario:(SharingScenario)scenario {
  DCHECK(additionalText);

  if ((self = [self initWithURL:URL title:title scenario:scenario])) {
    _additionalText = [additionalText copy];
  }
  return self;
}

@end
