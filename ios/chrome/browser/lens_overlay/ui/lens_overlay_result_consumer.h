// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULT_CONSUMER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULT_CONSUMER_H_

class GURL;

/// Consumer for lens overlay result selection UI.
@protocol LensOverlayResultConsumer

// Notifies the consumer that the a new Lens search request started.
- (void)handleSearchRequestStarted;

// Notifies the consumer that the Lens search request received an error.
- (void)handleSearchRequestErrored;

// Loads a new results URL with optional HTTP headers. Headers are cached and
// will be used for subsequent calls where `httpHeaders` is nil.
- (void)loadResultsURL:(GURL)url
           httpHeaders:(NSDictionary<NSString*, NSString*>*)httpHeaders;

// Notifies the consumer that Lens is trying to perform some network request,
// but it's taking longer than expected to receive a result.
- (void)handleSlowRequestHasStarted;

@end

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_UI_LENS_OVERLAY_RESULT_CONSUMER_H_
