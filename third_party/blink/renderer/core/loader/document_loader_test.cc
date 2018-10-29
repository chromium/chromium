// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/document_loader.h"

#include <queue>
#include "base/auto_reset.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/frame/frame_owner_element_type.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_url_loader_client.h"
#include "third_party/blink/public/platform/web_url_loader_mock_factory.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

namespace blink {

// TODO(dcheng): Ideally, enough of frame_test_helpers would be in core/ that
// placing a test for a core/ class in web/ wouldn't be necessary.
class DocumentLoaderTest : public testing::Test {
 protected:
  void SetUp() override {
    web_view_helper_.Initialize();
    url_test_helpers::RegisterMockedURLLoad(
        url_test_helpers::ToKURL("https://example.com/foo.html"),
        test::CoreTestDataPath("foo.html"));
  }

  void TearDown() override {
    Platform::Current()
        ->GetURLLoaderMockFactory()
        ->UnregisterAllURLsAndClearMemoryCache();
  }

  WebLocalFrameImpl* MainFrame() { return web_view_helper_.LocalMainFrame(); }

  frame_test_helpers::WebViewHelper web_view_helper_;
};

TEST_F(DocumentLoaderTest, SingleChunk) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";
      original_client->DidReceiveData(data, data_length);
    }
  } delegate;

  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(nullptr);

  // TODO(dcheng): How should the test verify that the original callback is
  // invoked? The test currently still passes even if the test delegate
  // forgets to invoke the callback.
}

// Test normal case of DocumentLoader::dataReceived(): data in multiple chunks,
// with no reentrancy.
TEST_F(DocumentLoaderTest, MultiChunkNoReentrancy) {
  class TestDelegate : public WebURLLoaderTestDelegate {
   public:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";
      // Chunk the reply into one byte chunks.
      for (int i = 0; i < data_length; ++i)
        original_client->DidReceiveData(&data[i], 1);
    }
  } delegate;

  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(&delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(nullptr);
}

// Finally, test reentrant callbacks to DocumentLoader::dataReceived().
TEST_F(DocumentLoaderTest, MultiChunkWithReentrancy) {
  // This test delegate chunks the response stage into three distinct stages:
  // 1. The first dataReceived() callback, which triggers frame detach due to
  //    commiting a provisional load.
  // 2.  The middle part of the response, which is dispatched to
  //    dataReceived() reentrantly.
  // 3. The final chunk, which is dispatched normally at the top-level.
  class ChildDelegate : public WebURLLoaderTestDelegate,
                        public frame_test_helpers::TestWebFrameClient {
   public:
    // WebURLLoaderTestDelegate overrides:
    void DidReceiveData(WebURLLoaderClient* original_client,
                        const char* data,
                        int data_length) override {
      EXPECT_EQ(34, data_length) << "foo.html was not served in a single chunk";

      loader_client_ = original_client;
      for (int i = 0; i < data_length; ++i)
        data_.push(data[i]);

      {
        // Serve the first byte to the real WebURLLoaderCLient, which
        // should trigger frameDetach() due to committing a provisional
        // load.
        base::AutoReset<bool> dispatching(&dispatching_did_receive_data_, true);
        DispatchOneByte();
      }
      // Serve the remaining bytes to complete the load.
      EXPECT_FALSE(data_.empty());
      while (!data_.empty())
        DispatchOneByte();
    }

    // WebLocalFrameClient overrides:
    void FrameDetached(DetachType detach_type) override {
      if (dispatching_did_receive_data_) {
        // This should be called by the first didReceiveData() call, since
        // it should commit the provisional load.
        EXPECT_GT(data_.size(), 10u);
        // Dispatch dataReceived() callbacks for part of the remaining
        // data, saving the rest to be dispatched at the top-level as
        // normal.
        while (data_.size() > 10)
          DispatchOneByte();
        served_reentrantly_ = true;
      }
      TestWebFrameClient::FrameDetached(detach_type);
    }

    void DispatchOneByte() {
      char c = data_.front();
      data_.pop();
      loader_client_->DidReceiveData(&c, 1);
    }

    bool ServedReentrantly() const { return served_reentrantly_; }

   private:
    WebURLLoaderClient* loader_client_ = nullptr;
    std::queue<char> data_;
    bool dispatching_did_receive_data_ = false;
    bool served_reentrantly_ = false;
  };

  class MainFrameClient : public frame_test_helpers::TestWebFrameClient {
   public:
    explicit MainFrameClient(TestWebFrameClient& child_client)
        : child_client_(child_client) {}
    WebLocalFrame* CreateChildFrame(WebLocalFrame* parent,
                                    WebTreeScopeType scope,
                                    const WebString& name,
                                    const WebString& fallback_name,
                                    WebSandboxFlags,
                                    const ParsedFeaturePolicy&,
                                    const WebFrameOwnerProperties&,
                                    FrameOwnerElementType) override {
      return CreateLocalChild(*parent, scope, &child_client_);
    }

   private:
    TestWebFrameClient& child_client_;
  };

  ChildDelegate child_delegate;
  MainFrameClient main_frame_client(child_delegate);
  web_view_helper_.Initialize(&main_frame_client);

  // This doesn't go through the mocked URL load path: it's just intended to
  // setup a situation where didReceiveData() can be invoked reentrantly.
  frame_test_helpers::LoadHTMLString(MainFrame(), "<iframe></iframe>",
                                     url_test_helpers::ToKURL("about:blank"));

  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(
      &child_delegate);
  frame_test_helpers::LoadFrame(MainFrame(), "https://example.com/foo.html");
  Platform::Current()->GetURLLoaderMockFactory()->SetLoaderDelegate(nullptr);

  EXPECT_TRUE(child_delegate.ServedReentrantly());

  // delegate is a WebLocalFrameClient and stack-allocated, so manually reset()
  // the WebViewHelper here.
  web_view_helper_.Reset();
}

TEST_F(DocumentLoaderTest, isCommittedButEmpty) {
  WebViewImpl* web_view_impl =
      web_view_helper_.InitializeAndLoad("about:blank");
  EXPECT_TRUE(ToLocalFrame(web_view_impl->GetPage()->MainFrame())
                  ->Loader()
                  .GetDocumentLoader()
                  ->IsCommittedButEmpty());
}

}  // namespace blink
