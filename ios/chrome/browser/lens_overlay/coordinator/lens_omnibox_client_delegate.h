// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OMNIBOX_CLIENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OMNIBOX_CLIENT_DELEGATE_H_

#import <string>

class GURL;

/// Delegate for LensOmniboxClient.
@protocol LensOmniboxClientDelegate

/// Omnibox did accept a suggestion with `text` and `destinationURL`.
- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
            thumbnailRemoved:(BOOL)thumbnailRemoved;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_COORDINATOR_LENS_OMNIBOX_CLIENT_DELEGATE_H_
