// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_OMNIBOX_CLIENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_OMNIBOX_CLIENT_DELEGATE_H_

/// Delegate for AIMOmniboxClient.
@protocol AIMOmniboxClientDelegate

/// Omnibox did accept a suggestion with `text` and `destinationURL`.
/// `isSearchType`: Whether the search type is text or a URL.
- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
                isSearchType:(BOOL)isSearchType;

/// Omnibox did change text.
- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_COORDINATOR_AIM_OMNIBOX_CLIENT_DELEGATE_H_
