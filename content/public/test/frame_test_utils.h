// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FRAME_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_FRAME_TEST_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

#include "content/public/test/browser_test_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace content {

// Returns the text contents of the leaf frame. `frame_tree` contains the query
// string to be passed to cross_site_iframe_factory, where the desired leaf
// frame is some URL that ends up at `/echoheader?Cookie`. `leaf_path` is the
// sequence of frame indices to choose when traversing from the top level frame
// to the leaf frame.
std::string ArrangeFramesAndGetContentFromLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    std::string_view frame_tree,
    const std::vector<int>& leaf_path);

// Returns the cookies for a specified `cookie_url`.
std::vector<net::CanonicalCookie> ArrangeFramesAndGetCanonicalCookiesForLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    std::string_view frame_tree,
    const GURL& cookie_url);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FRAME_TEST_UTILS_H_
