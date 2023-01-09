// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PDF_TEST_MOCK_WEB_ASSOCIATED_URL_LOADER_H_
#define PDF_TEST_MOCK_WEB_ASSOCIATED_URL_LOADER_H_

#include "base/task/single_thread_task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"

namespace chrome_pdf {

class MockWebAssociatedURLLoader : public blink::WebAssociatedURLLoader {
 public:
  MockWebAssociatedURLLoader();
  MockWebAssociatedURLLoader(const MockWebAssociatedURLLoader&) = delete;
  MockWebAssociatedURLLoader& operator=(const MockWebAssociatedURLLoader&) =
      delete;
  ~MockWebAssociatedURLLoader() override;

  MOCK_METHOD(void,
              LoadAsynchronously,
              (const blink::WebURLRequest&,
               blink::WebAssociatedURLLoaderClient*),
              (override));
  MOCK_METHOD(void, Cancel, (), (override));
  MOCK_METHOD(void, SetDefersLoading, (bool), (override));
  MOCK_METHOD(void,
              SetLoadingTaskRunner,
              (base::SingleThreadTaskRunner*),
              (override));
};

}  // namespace chrome_pdf

#endif  // PDF_TEST_MOCK_WEB_ASSOCIATED_URL_LOADER_H_
