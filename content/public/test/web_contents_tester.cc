// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/web_contents_tester.h"

#include <utility>

#include "content/test/test_web_contents.h"

namespace content {

namespace {

// The two subclasses here are instantiated via the deprecated
// CreateWebContentsFor... factories below.

}  // namespace

// static
WebContentsTester* WebContentsTester::For(WebContents* contents) {
  return static_cast<TestWebContents*>(contents);
}

// static
std::unique_ptr<WebContents> WebContentsTester::CreateTestWebContents(
    BrowserContext* browser_context,
    scoped_refptr<SiteInstance> instance) {
  return TestWebContents::Create(browser_context, std::move(instance));
}

// static
WebContents* WebContentsTester::CreateTestWebContents(
    const WebContents::CreateParams& params) {
  return TestWebContents::Create(params);
}

}  // namespace content
