// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/ppapi_migration/url_loader.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/test/mock_callback.h"
#include "pdf/ppapi_migration/callback.h"
#include "ppapi/c/pp_errors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_associated_url_loader_client.h"
#include "third_party/blink/public/web/web_associated_url_loader_options.h"

namespace chrome_pdf {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::NiceMock;
using ::testing::ReturnNull;

class MockWebAssociatedURLLoader : public blink::WebAssociatedURLLoader {
 public:
  // blink::WebAssociatedURLLoader:
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

class MockBlinkUrlLoaderClient : public BlinkUrlLoader::Client {
 public:
  base::WeakPtr<MockBlinkUrlLoaderClient> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  void InvalidateWeakPtrs() { weak_factory_.InvalidateWeakPtrs(); }

  // BlinkUrlLoader::Client:
  MOCK_METHOD(std::unique_ptr<blink::WebAssociatedURLLoader>,
              CreateAssociatedURLLoader,
              (const blink::WebAssociatedURLLoaderOptions&),
              (override));

 private:
  base::WeakPtrFactory<MockBlinkUrlLoaderClient> weak_factory_{this};
};

class BlinkUrlLoaderTest : public testing::Test {
 protected:
  BlinkUrlLoaderTest() {
    ON_CALL(mock_client_, CreateAssociatedURLLoader(_))
        .WillByDefault(
            Invoke(this, &BlinkUrlLoaderTest::FakeCreateAssociatedURLLoader));
    loader_ = base::MakeRefCounted<BlinkUrlLoader>(mock_client_.GetWeakPtr());
  }

  std::unique_ptr<blink::WebAssociatedURLLoader> FakeCreateAssociatedURLLoader(
      const blink::WebAssociatedURLLoaderOptions& options) {
    EXPECT_TRUE(mock_url_loader_);
    saved_options_ = options;
    return std::move(mock_url_loader_);
  }

  NiceMock<MockBlinkUrlLoaderClient> mock_client_;
  base::MockCallback<ResultCallback> mock_callback_;
  scoped_refptr<BlinkUrlLoader> loader_;

  std::unique_ptr<MockWebAssociatedURLLoader> mock_url_loader_ =
      std::make_unique<MockWebAssociatedURLLoader>();
  blink::WebAssociatedURLLoaderOptions saved_options_;
};

TEST_F(BlinkUrlLoaderTest, GrantUniversalAccess) {
  loader_->GrantUniversalAccess();
  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_TRUE(saved_options_.grant_universal_access);
}

TEST_F(BlinkUrlLoaderTest, Open) {
  EXPECT_CALL(mock_client_, CreateAssociatedURLLoader(_));
  EXPECT_CALL(mock_callback_, Run(_)).Times(0);

  loader_->Open(UrlRequest(), mock_callback_.Get());
  EXPECT_FALSE(saved_options_.grant_universal_access);
}

TEST_F(BlinkUrlLoaderTest, OpenWithInvalidatedClient) {
  EXPECT_CALL(mock_client_, CreateAssociatedURLLoader(_)).Times(0);
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  mock_client_.InvalidateWeakPtrs();
  loader_->Open(UrlRequest(), mock_callback_.Get());
}

TEST_F(BlinkUrlLoaderTest, OpenWithFailingCreateAssociatedURLLoader) {
  EXPECT_CALL(mock_client_, CreateAssociatedURLLoader(_))
      .WillOnce(ReturnNull());
  EXPECT_CALL(mock_callback_, Run(PP_ERROR_FAILED));

  loader_->Open(UrlRequest(), mock_callback_.Get());
}

}  // namespace
}  // namespace chrome_pdf
