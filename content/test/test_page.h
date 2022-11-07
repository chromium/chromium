// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_PAGE_H_
#define CONTENT_TEST_TEST_PAGE_H_

#include "content/browser/renderer_host/page_impl.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"

namespace content {

class PageDelegate;
class RenderFrameHostImpl;

class TestPage : public PageImpl {
 public:
  TestPage(RenderFrameHostImpl& rfh, PageDelegate& delegate);

  TestPage(const TestPage&) = delete;
  TestPage& operator=(const TestPage&) = delete;

  ~TestPage() override;

  const absl::optional<GURL>& GetManifestUrl() const override;

  void UpdateManifestUrl(const GURL& manifest_url) override;

 private:
  absl::optional<GURL> manifest_url_;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_PAGE_H_
