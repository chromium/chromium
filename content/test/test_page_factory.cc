// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/test_page_factory.h"

#include "content/test/test_page.h"

namespace content {

TestPageFactory::TestPageFactory() {
  PageFactory::RegisterFactory(this);
}

TestPageFactory::~TestPageFactory() {
  PageFactory::UnregisterFactory();
}

std::unique_ptr<PageImpl> TestPageFactory::CreatePage(RenderFrameHostImpl& rfh,
                                                      PageDelegate& delegate) {
  return std::make_unique<TestPage>(rfh, delegate);
}

}  // namespace content
