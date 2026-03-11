// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_URL_LOADER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_URL_LOADER_H_

struct UrlLoadParams;

// Protocol for loading URLs in the composebox.
@protocol ComposeboxURLLoader

// Prepares for loading a new query text.
- (void)prepareLoadForQueryText:(NSString*)queryText;

- (void)loadURLParams:(const UrlLoadParams&)URLLoadParams;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_COORDINATOR_COMPOSEBOX_URL_LOADER_H_
