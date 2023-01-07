// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_FRAME_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_FRAME_TEST_UTILS_H_

#include <vector>

#include "content/public/test/browser_test_utils.h"
#include "net/cookies/canonical_cookie.h"
#include "url/gurl.h"

namespace content {

// Returns the text contents of the leaf frame. `frame_tree_pattern` contains a
// format template for the query string to be passed to
// cross_site_iframe_factory, where the URL of the desired leaf frame is
// `leaf_url`, e.g. some URL that ends up at `/echoheader?Cookie`. `leaf_path`
// is the sequence of frame indices to choose when traversing from the top level
// frame to the leaf frame.
std::string ArrangeFramesAndGetContentFromLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    const std::string& frame_tree_pattern,
    const std::vector<int>& leaf_path,
    const GURL& leaf_url);

// Returns the cookies for the leaf's origin.
// `frame_tree_pattern` contains a format template for the query string to be
// passed to cross_site_iframe_factory, where the URL of the desired leaf
// frame is `leaf_url`, e.g. some URL that ends up at `/set-cookie`.
std::vector<net::CanonicalCookie> ArrangeFramesAndGetCanonicalCookiesForLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    const std::string& frame_tree_pattern,
    const GURL& leaf_url);

// Same as above but returns cookies for a separately specified `cookie_url`.
std::vector<net::CanonicalCookie> ArrangeFramesAndGetCanonicalCookiesForLeaf(
    WebContents* web_contents,
    net::EmbeddedTestServer* server,
    const std::string& frame_tree_pattern,
    const GURL& leaf_url,
    const GURL& cookie_url);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_FRAME_TEST_UTILS_H_