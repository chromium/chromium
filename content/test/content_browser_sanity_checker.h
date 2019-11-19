// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_BROWSER_SANITY_CHECKER_H_
#define CONTENT_TEST_CONTENT_BROWSER_SANITY_CHECKER_H_

#include "base/callback.h"
#include "base/macros.h"

namespace content {

class WebContents;

// While an instance of this class exists, a bunch of sanity-checks are enabled
// to validate the correctness of the browser side of the content layer
// implementation. It's good to enable these in both unit tests and browser
// tests, as a means of detecting bugs in the implementation of the content api.
//
// These checks should typically be enabled by the base class of any test
// framework that would instantiate a WebContents -- ContentBrowserTest::SetUp,
// RenderViewHostTestHarness::SetUp, and so forth. Individual tests won't
// typically need to enable them.
//
// For the nuts and bolts of what the checks enforce, see the implementation.
class ContentBrowserSanityChecker {
 public:
  ContentBrowserSanityChecker();
  ~ContentBrowserSanityChecker();

 private:
  void OnWebContentsCreated(WebContents* web_contents);

  // The callback needs to be cached so that it can be unregistered.
  base::RepeatingCallback<void(WebContents*)> creation_hook_;

  DISALLOW_COPY_AND_ASSIGN(ContentBrowserSanityChecker);
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_BROWSER_SANITY_CHECKER_H_
