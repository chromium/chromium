// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_CONTENT_TEST_SUITE_H_
#define CONTENT_TEST_CONTENT_TEST_SUITE_H_

#include <memory>

#include "base/test/test_discardable_memory_allocator.h"
#include "build/build_config.h"
#include "content/public/test/content_test_suite_base.h"

#if defined(OS_WIN)
#include "base/win/scoped_com_initializer.h"
#endif

namespace content {

class TestContentClientInitializer;

class ContentTestSuite : public ContentTestSuiteBase {
 public:
  ContentTestSuite(int argc, char** argv);

  ContentTestSuite(const ContentTestSuite&) = delete;
  ContentTestSuite& operator=(const ContentTestSuite&) = delete;

  ~ContentTestSuite() override;

 protected:
  void Initialize() override;
  void Shutdown() override;

 private:
  std::unique_ptr<TestContentClientInitializer>
      test_content_client_initializer_;
  base::TestDiscardableMemoryAllocator discardable_memory_allocator_;

#if defined(OS_WIN)
  base::win::ScopedCOMInitializer com_initializer_;
#endif
};

}  // namespace content

#endif  // CONTENT_TEST_CONTENT_TEST_SUITE_H_
