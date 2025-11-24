// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_OMNIBOX_CLIENT_DELEGATE_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_OMNIBOX_CLIENT_DELEGATE_H_

struct UrlLoadParams;
enum class WindowOpenDisposition;

/// Delegate for ComposeboxOmniboxClient.
@protocol ComposeboxOmniboxClientDelegate

/// Returns the current attached suggest input in the composebox.
- (std::optional<lens::proto::LensOverlaySuggestInputs>)suggestInputs;

/// Returns YES if AI Mode is enabled.
- (BOOL)isAIModeEnabled;

/// Omnibox did accept a suggestion with `text` and `destinationURL`.
/// `isSearchType`: Whether the search type is text or a URL.
- (void)omniboxDidAcceptText:(const std::u16string&)text
              destinationURL:(const GURL&)destinationURL
               URLLoadParams:(const UrlLoadParams&)URLLoadParams
                isSearchType:(BOOL)isSearchType;

/// Omnibox did change text.
- (void)omniboxDidChangeText:(const std::u16string&)text
               isSearchQuery:(BOOL)isSearchQuery
         userInputInProgress:(BOOL)userInputInProgress;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_OMNIBOX_CLIENT_DELEGATE_H_
