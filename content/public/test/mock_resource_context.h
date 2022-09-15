// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_MOCK_RESOURCE_CONTEXT_H_
#define CONTENT_PUBLIC_TEST_MOCK_RESOURCE_CONTEXT_H_

#include "content/public/browser/resource_context.h"

namespace content {

class MockResourceContext : public ResourceContext {
 public:
  MockResourceContext();

  MockResourceContext(const MockResourceContext&) = delete;
  MockResourceContext& operator=(const MockResourceContext&) = delete;

  ~MockResourceContext() override;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_MOCK_RESOURCE_CONTEXT_H_
