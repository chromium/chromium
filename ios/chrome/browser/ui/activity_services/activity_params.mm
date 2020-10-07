// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/activity_services/activity_params.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ActivityParams

- (instancetype)initWithScenario:(ActivityScenario)scenario {
  if (self = [super init]) {
    _scenario = scenario;
  }
  return self;
}

- (instancetype)initWithImage:(UIImage*)image
                        title:(NSString*)title
                     scenario:(ActivityScenario)scenario {
  DCHECK(image);
  DCHECK(title);
  if (self = [self initWithScenario:scenario]) {
    _image = image;
    _title = title;
  }
  return self;
}

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
                   scenario:(ActivityScenario)scenario {
  DCHECK(URL.is_valid());
  DCHECK(title);
  if (self = [self initWithScenario:scenario]) {
    _URL = URL;
    _title = title;
  }
  return self;
}

- (instancetype)initWithURL:(const GURL&)URL
                      title:(NSString*)title
             additionalText:(NSString*)additionalText
                   scenario:(ActivityScenario)scenario {
  DCHECK(additionalText);
  if (self = [self initWithURL:URL title:title scenario:scenario]) {
    _additionalText = [additionalText copy];
  }
  return self;
}

@end
