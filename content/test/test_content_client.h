// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_TEST_CONTENT_CLIENT_H_
#define CONTENT_TEST_TEST_CONTENT_CLIENT_H_

#include <string_view>

#include "content/public/common/content_client.h"

namespace content {

class TestContentClient : public ContentClient {
 public:
  TestContentClient();

  TestContentClient(const TestContentClient&) = delete;
  TestContentClient& operator=(const TestContentClient&) = delete;

  ~TestContentClient() override;

  // ContentClient:
  std::string_view GetDataResource(
      int resource_id,
      ui::ResourceScaleFactor scale_factor) override;
  base::RefCountedMemory* GetDataResourceBytes(int resource_id) override;
  std::string GetDataResourceString(int resource_id) override;
};

}  // namespace content

#endif  // CONTENT_TEST_TEST_CONTENT_CLIENT_H_
