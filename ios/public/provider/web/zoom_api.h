// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_WEB_ZOOM_API_H_
#define IOS_PUBLIC_PROVIDER_WEB_ZOOM_API_H_

@class WKWebView;

namespace web::provider {

// Sets the zoom size (as a percentage, e.g. 150 for 150%) on `web_view`
// if supported by the provider. Returns whether the zoom was set.
bool SetZoomForWebView(WKWebView* web_view, int size);

}  // namespace web::provider

#endif  // IOS_PUBLIC_PROVIDER_WEB_ZOOM_API_H_
