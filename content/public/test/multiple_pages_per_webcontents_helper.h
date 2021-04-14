// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MULTIPLE_PAGES_PER_WEBCONTENTS_HELPER_H_
#define CONTENT_PUBLIC_TEST_MULTIPLE_PAGES_PER_WEBCONTENTS_HELPER_H_

#include <memory>

namespace content {

class NavigationController;
class RenderFrameHost;
class WebContents;

// TODO(https://crbug.com/1196381): Refactor tests to use a prerender page host
// for tests. The TestPageHolder owns the FrameTree in the test framework and
// allows the creation of more than one FrameTree in a WebContents.
class TestPageHolder {
 public:
  virtual ~TestPageHolder() = default;
  virtual RenderFrameHost* GetMainFrame() = 0;
  virtual NavigationController& GetController() = 0;
};

std::unique_ptr<TestPageHolder> CreatePageHolderForTests(
    WebContents* web_contents);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MULTIPLE_PAGES_PER_WEBCONTENTS_HELPER_H_
