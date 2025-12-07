// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_EGTEST_UTIL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_EGTEST_UTIL_H_

#import <memory>

@protocol GREYMatcher;
namespace net::test_server {
struct HttpRequest;
class HttpResponse;
}  // namespace net::test_server

namespace download {

// Matcher for "Download" button on Download Manager UI.
id<GREYMatcher> DownloadButton();

// Provides downloads landing page with download link.
std::unique_ptr<net::test_server::HttpResponse> GetResponse(
    const net::test_server::HttpRequest& request);

// Provides test page for new page downloads with content disposition.
std::unique_ptr<net::test_server::HttpResponse>
GetLinkToContentDispositionResponse(
    const net::test_server::HttpRequest& request);

// Provides test page for downloads with content disposition.
std::unique_ptr<net::test_server::HttpResponse>
GetContentDispositionPDFResponse(const net::test_server::HttpRequest& request);

// Waits until Open in... button is shown.
[[nodiscard]] bool WaitForOpenInButton();

// Waits until `OPEN` button is shown.
[[nodiscard]] bool WaitForOpenPDFButton();

// Waits until Download button is shown.
[[nodiscard]] bool WaitForDownloadButton(bool loading);

}  // namespace download

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_EGTEST_UTIL_H_
