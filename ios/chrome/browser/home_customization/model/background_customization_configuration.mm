// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/home_customization/model/background_customization_configuration.h"

#import "url/gurl.h"

@implementation BackgroundCustomizationConfiguration {
  GURL _thumbnailURL;
  GURL _highResURL;
}

#pragma mark - Properties

- (const GURL&)thumbnailURL {
  return _thumbnailURL;
}

- (void)setThumbnailURL:(const GURL&)url {
  _thumbnailURL = url;
}

- (const GURL&)highResURL {
  return _highResURL;
}

- (void)setHighResURL:(const GURL&)url {
  _highResURL = url;
}

@end
