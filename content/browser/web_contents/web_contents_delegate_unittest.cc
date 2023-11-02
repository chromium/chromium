// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_delegate.h"

#include <memory>

#include "base/compiler_specific.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/test_render_view_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class MockWebContentsDelegate : public WebContentsDelegate {
};

class WebContentsDelegateTest : public RenderViewHostImplTestHarness {
};

TEST_F(WebContentsDelegateTest, UnregisterInDestructor) {
  std::unique_ptr<WebContents> contents_a(
      WebContents::Create(WebContents::CreateParams(browser_context())));
  WebContentsImpl* raw_contents_a =
      static_cast<WebContentsImpl*>(contents_a.get());
  std::unique_ptr<WebContents> contents_b(
      WebContents::Create(WebContents::CreateParams(browser_context())));
  WebContentsImpl* raw_contents_b =
      static_cast<WebContentsImpl*>(contents_b.get());
  EXPECT_EQ(nullptr, raw_contents_a->GetDelegate());
  EXPECT_EQ(nullptr, raw_contents_b->GetDelegate());

  std::unique_ptr<MockWebContentsDelegate> delegate(
      new MockWebContentsDelegate());

  // Setting a delegate should work correctly.
  raw_contents_a->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), raw_contents_a->GetDelegate());
  EXPECT_TRUE(raw_contents_b->GetDelegate() == nullptr);

  // A delegate can be a delegate to multiple WebContentsImpl.
  raw_contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), raw_contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), raw_contents_b->GetDelegate());

  // Setting the same delegate multiple times should work correctly.
  raw_contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), raw_contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), raw_contents_b->GetDelegate());

  // Setting delegate to NULL should work correctly.
  raw_contents_b->SetDelegate(nullptr);
  EXPECT_EQ(delegate.get(), raw_contents_a->GetDelegate());
  EXPECT_TRUE(raw_contents_b->GetDelegate() == nullptr);

  // Destroying the delegate while it is still the delegate for a
  // WebContentsImpl should unregister it.
  raw_contents_b->SetDelegate(delegate.get());
  EXPECT_EQ(delegate.get(), raw_contents_a->GetDelegate());
  EXPECT_EQ(delegate.get(), raw_contents_b->GetDelegate());
  delegate.reset(nullptr);
  EXPECT_TRUE(raw_contents_a->GetDelegate() == nullptr);
  EXPECT_TRUE(raw_contents_b->GetDelegate() == nullptr);

  // Destroy the WebContentses and run the message loop to prevent leaks.
  contents_a.reset();
  contents_b.reset();
}

}  // namespace content
