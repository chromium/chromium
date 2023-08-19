// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/save_image_to_photos_command.h"

#import "ios/web/public/navigation/referrer.h"
#import "ios/web/public/web_state.h"
#import "url/gurl.h"

@implementation SaveImageToPhotosCommand

- (instancetype)initWithImageURL:(GURL)imageURL
                        referrer:(web::Referrer)referrer
                        webState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _imageURL = imageURL;
    _referrer = referrer;
    _webState = webState->GetWeakPtr();
  }
  return self;
}

@end
