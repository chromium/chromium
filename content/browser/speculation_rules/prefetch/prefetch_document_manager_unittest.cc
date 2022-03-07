// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_document_manager.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchDocumentManagerTest : public RenderViewHostTestHarness {};

TEST_F(PrefetchDocumentManagerTest, CreateDocumentManager) {
  // Create a PrefetchDocumentManager, and ensure it was created properly.
  PrefetchDocumentManager::CreateForCurrentDocument(
      web_contents()->GetMainFrame());
  EXPECT_TRUE(PrefetchDocumentManager::GetForCurrentDocument(
      web_contents()->GetMainFrame()));
}

}  // namespace
}  // namespace content
