// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_PAGE_FACTORY_H_
#define CONTENT_TEST_TEST_PAGE_FACTORY_H_

#include "content/browser/renderer_host/page_factory.h"

namespace content {

// Manages creation of the PageImpls; when registered, all created PageImpls
// will be TestPages. This automatically registers itself when it goes in
// scope, and unregisters itself when it goes out of scope. Since you can't
// have more than one factory registered at a time, you can only have one of
// these objects at a time.
class TestPageFactory : public PageFactory {
 public:
  TestPageFactory();

  TestPageFactory(const TestPageFactory&) = delete;
  TestPageFactory& operator=(const TestPageFactory&) = delete;

  ~TestPageFactory() override;

 protected:
  // PageFactory implementation.
  std::unique_ptr<PageImpl> CreatePage(RenderFrameHostImpl& rfh,
                                       PageDelegate& delegate) override;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_PAGE_FACTORY_H_
