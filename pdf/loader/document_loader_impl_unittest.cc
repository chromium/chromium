// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/loader/document_loader_impl.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "pdf/loader/url_loader_wrapper.h"
#include "pdf/pdf_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/range/range.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::Sequence;

namespace chrome_pdf {

namespace {

constexpr uint32_t kDefaultRequestSize =
    DocumentLoaderImpl::kDefaultRequestSize;

class TestURLLoader : public URLLoaderWrapper {
 public:
  class LoaderData {
   public:
    LoaderData() = default;
    LoaderData(const LoaderData&) = delete;
    LoaderData& operator=(const LoaderData&) = delete;
    ~LoaderData() {
      // We should call callbacks to prevent memory leaks.
      // The callbacks don't do anything, because the objects that created the
      // callbacks have been destroyed.
      if (IsWaitRead())
        CallReadCallback(-1);
      if (IsWaitOpen())
        CallOpenCallback(-1);
    }

    int content_length() const { return content_length_; }
    void set_content_length(int content_length) {
      content_length_ = content_length;
    }
    bool accept_ranges_bytes() const { return accept_ranges_bytes_; }
    void set_accept_ranges_bytes(bool accept_ranges_bytes) {
      accept_ranges_bytes_ = accept_ranges_bytes;
    }
    bool content_encoded() const { return content_encoded_; }
    void set_content_encoded(bool content_encoded) {
      content_encoded_ = content_encoded;
    }
    const std::string& content_type() const { return content_type_; }
    void set_content_type(const std::string& content_type) {
      content_type_ = content_type;
    }
    const std::string& content_disposition() const {
      return content_disposition_;
    }
    void set_content_disposition(const std::string& content_disposition) {
      content_disposition_ = content_disposition;
    }
    const std::string& multipart_boundary() const {
      return multipart_boundary_;
    }
    void set_multipart_boundary(const std::string& multipart_boundary) {
      multipart_boundary_ = multipart_boundary;
    }
    const gfx::Range& byte_range() const { return byte_range_; }
    void set_byte_range(const gfx::Range& byte_range) {
      byte_range_ = byte_range;
    }
    bool is_multipart() const { return is_multipart_; }
    void set_is_multipart(bool is_multipart) { is_multipart_ = is_multipart; }
    int status_code() const { return status_code_; }
    void set_status_code(int status_code) { status_code_ = status_code; }
    bool closed() const { return closed_; }
    void set_closed(bool closed) { closed_ = closed; }
    const gfx::Range& open_byte_range() const { return open_byte_range_; }
    void set_open_byte_range(const gfx::Range& open_byte_range) {
      open_byte_range_ = open_byte_range;
    }

    bool IsWaitRead() const { return !did_read_callback_.is_null(); }
    bool IsWaitOpen() const { return !did_open_callback_.is_null(); }

    void SetReadCallback(base::OnceCallback<void(int)> read_callback) {
      did_read_callback_ = std::move(read_callback);
    }

    void SetOpenCallback(base::OnceCallback<void(int)> open_callback,
                         gfx::Range req_byte_range) {
      did_open_callback_ = std::move(open_callback);
      set_open_byte_range(req_byte_range);
    }

    void CallOpenCallback(int result) {
      DCHECK(IsWaitOpen());
      std::move(did_open_callback_).Run(result);
    }

    void CallReadCallback(int result) {
      DCHECK(IsWaitRead());
      std::move(did_read_callback_).Run(result);
    }

   private:
    base::OnceCallback<void(int)> did_open_callback_;
    base::OnceCallback<void(int)> did_read_callback_;

    int content_length_ = -1;
    bool accept_ranges_bytes_ = false;
    bool content_encoded_ = false;
    std::string content_type_;
    std::string content_disposition_;
    std::string multipart_boundary_;
    gfx::Range byte_range_ = gfx::Range::InvalidRange();
    bool is_multipart_ = false;
    int status_code_ = 0;
    bool closed_ = true;
    gfx::Range open_byte_range_ = gfx::Range::InvalidRange();
  };

  explicit TestURLLoader(LoaderData* data) : data_(data) {
    data_->set_closed(false);
  }
  TestURLLoader(const TestURLLoader&) = delete;
  TestURLLoader& operator=(const TestURLLoader&) = delete;
  ~TestURLLoader() override { Close(); }

  int GetContentLength() const override { return data_->content_length(); }

  bool IsAcceptRangesBytes() const override {
    return data_->accept_ranges_bytes();
  }

  bool IsContentEncoded() const override { return data_->content_encoded(); }

  std::string GetContentType() const override { return data_->content_type(); }

  std::string GetContentDisposition() const override {
    return data_->content_disposition();
  }

  int GetStatusCode() const override { return data_->status_code(); }

  bool IsMultipart() const override { return data_->is_multipart(); }

  bool GetByteRangeStart(int* start) const override {
    *start = data_->byte_range().start();
    return data_->byte_range().IsValid();
  }

  void Close() override { data_->set_closed(true); }

  void OpenRange(const std::string& url,
                 const std::string& referrer_url,
                 uint32_t position,
                 uint32_t size,
                 base::OnceCallback<void(int)> callback) override {
    data_->SetOpenCallback(std::move(callback),
                           gfx::Range(position, position + size));
  }

  void ReadResponseBody(base::span<char> /*buffer*/,
                        base::OnceCallback<void(int)> callback) override {
    data_->SetReadCallback(std::move(callback));
  }

 private:
  raw_ptr<LoaderData> data_;
};

class TestClient : public DocumentLoader::Client {
 public:
  TestClient() { full_page_loader_data()->set_content_type("application/pdf"); }
  TestClient(const TestClient&) = delete;
  TestClient& operator=(const TestClient&) = delete;
  ~TestClient() override = default;

  // DocumentLoader::Client overrides:
  std::unique_ptr<URLLoaderWrapper> CreateURLLoader() override {
    return std::unique_ptr<URLLoaderWrapper>(
        new TestURLLoader(partial_loader_data()));
  }
  void OnPendingRequestComplete() override {}
  void OnNewDataReceived() override {}
  void OnDocumentComplete() override {}
  void OnDocumentCanceled() override {}

  std::unique_ptr<URLLoaderWrapper> CreateFullPageLoader() {
    return std::unique_ptr<URLLoaderWrapper>(
        new TestURLLoader(full_page_loader_data()));
  }

  TestURLLoader::LoaderData* full_page_loader_data() {
    return &full_page_loader_data_;
  }
  TestURLLoader::LoaderData* partial_loader_data() {
    return &partial_loader_data_;
  }

  void SetCanUsePartialLoading() {
    full_page_loader_data()->set_content_length(10 * 1024 * 1024);
    full_page_loader_data()->set_content_encoded(false);
    full_page_loader_data()->set_accept_ranges_bytes(true);
  }

  void SendAllPartialData() {
    partial_loader_data_.set_byte_range(partial_loader_data_.open_byte_range());
    partial_loader_data_.CallOpenCallback(0);
    uint32_t length = partial_loader_data_.byte_range().length();
    while (length > 0) {
      constexpr uint32_t max_part_len = kDefaultRequestSize;
      const uint32_t part_len = std::min(length, max_part_len);
      partial_loader_data_.CallReadCallback(part_len);
      length -= part_len;
    }
    if (partial_loader_data_.IsWaitRead()) {
      partial_loader_data_.CallReadCallback(0);
    }
  }

 private:
  TestURLLoader::LoaderData full_page_loader_data_;
  TestURLLoader::LoaderData partial_loader_data_;
};

class MockClient : public TestClient {
 public:
  MockClient() = default;
  MockClient(const MockClient&) = delete;
  MockClient& operator=(const MockClient&) = delete;

  MOCK_METHOD(void, OnPendingRequestComplete, (), (override));
  MOCK_METHOD(void, OnNewDataReceived, (), (override));
  MOCK_METHOD(void, OnDocumentComplete, (), (override));
  MOCK_METHOD(void, OnDocumentCanceled, (), (override));
};

}  // namespace

class DocumentLoaderImplTest : public testing::Test {
 protected:
  DocumentLoaderImplTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kPdfPartialLoading);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(DocumentLoaderImplTest, PartialLoadingFeatureDefault) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.Init();

  // Test that partial loading is disabled when feature is defaulted.
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingFeatureDisabled) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitAndDisableFeature(features::kPdfPartialLoading);

  // Test that partial loading is disabled when feature is disabled.
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingEnabled) {
  // Test that partial loading is enabled. (Fixture enables PdfPartialLoading.)
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingDisabledOnSmallFiles) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 2);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingDisabledIfContentEncoded) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_encoded(true);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingDisabledNoAcceptRangeBytes) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_accept_ranges_bytes(false);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingReallyDisabledRequestFromBegin) {
  TestClient client;
  DocumentLoaderImpl loader(&client);
  client.SetCanUsePartialLoading();
  loader.SetPartialLoadingEnabled(false);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  // We should not start partial loading if requested data is beside full page
  // loading position.
  loader.RequestData(kDefaultRequestSize, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingReallyDisabledRequestFromMiddle) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.SetPartialLoadingEnabled(false);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(1000000, 1);
  EXPECT_FALSE(loader.is_partial_loader_active());
  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingSimple) {
  TestClient client;
  client.SetCanUsePartialLoading();

  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  // While we have no requests, we should not start partial loading.
  EXPECT_FALSE(loader.is_partial_loader_active());

  loader.RequestData(5000000, 1);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_FALSE(loader.is_partial_loader_active());

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  // Partial loader should request headers.
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(loader.is_partial_loader_active());
  // Loader should be stopped.
  EXPECT_TRUE(client.full_page_loader_data()->closed());

  EXPECT_EQ("{4980736,10485760}",
            client.partial_loader_data()->open_byte_range().ToString());
}

TEST_F(DocumentLoaderImplTest, PartialLoadingBackOrder) {
  TestClient client;
  client.SetCanUsePartialLoading();

  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  // While we have no requests, we should not start partial loading.
  EXPECT_FALSE(loader.is_partial_loader_active());

  loader.RequestData(client.full_page_loader_data()->content_length() - 1, 1);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_FALSE(loader.is_partial_loader_active());

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  // Partial loader should request headers.
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(loader.is_partial_loader_active());
  // Loader should be stopped.
  EXPECT_TRUE(client.full_page_loader_data()->closed());

  // Requested range should be enlarged.
  EXPECT_GT(client.partial_loader_data()->open_byte_range().length(), 1u);
  EXPECT_EQ("{9830400,10485760}",
            client.partial_loader_data()->open_byte_range().ToString());
}

TEST_F(DocumentLoaderImplTest, CompleteWithoutPartial) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  while (client.full_page_loader_data()->IsWaitRead()) {
    client.full_page_loader_data()->CallReadCallback(1000);
  }
  EXPECT_TRUE(loader.IsDocumentComplete());
  EXPECT_TRUE(client.full_page_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, ErrorDownloadFullDocument) {
  TestClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_TRUE(client.full_page_loader_data()->IsWaitRead());
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  client.full_page_loader_data()->CallReadCallback(-3);
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(loader.IsDocumentComplete());
}

TEST_F(DocumentLoaderImplTest, CompleteNoContentLength) {
  TestClient client;
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(client.full_page_loader_data()->IsWaitRead());
    client.full_page_loader_data()->CallReadCallback(1000);
  }
  EXPECT_TRUE(client.full_page_loader_data()->IsWaitRead());
  client.full_page_loader_data()->CallReadCallback(0);
  EXPECT_EQ(10000ul, loader.GetDocumentSize());
  EXPECT_TRUE(loader.IsDocumentComplete());
  EXPECT_TRUE(client.full_page_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, CompleteWithPartial) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(19 * kDefaultRequestSize, kDefaultRequestSize);
  EXPECT_FALSE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(client.partial_loader_data()->IsWaitRead());
  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(client.partial_loader_data()->closed());

  client.SendAllPartialData();
  // Now we should send other document data.
  client.SendAllPartialData();
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, PartialRequestLastChunk) {
  constexpr uint32_t kLastChunkSize = 300;
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20 +
                                                     kLastChunkSize);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(20 * kDefaultRequestSize, 1);

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_EQ(
      static_cast<int>(client.partial_loader_data()->open_byte_range().end()),
      client.full_page_loader_data()->content_length());
  client.partial_loader_data()->set_byte_range(
      client.partial_loader_data()->open_byte_range());
  client.partial_loader_data()->CallOpenCallback(0);
  uint32_t data_length = client.partial_loader_data()->byte_range().length();
  while (data_length > kDefaultRequestSize) {
    client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
    data_length -= kDefaultRequestSize;
  }
  EXPECT_EQ(kLastChunkSize, data_length);
  client.partial_loader_data()->CallReadCallback(kLastChunkSize);
  EXPECT_TRUE(loader.IsDataAvailable(kDefaultRequestSize * 20, kLastChunkSize));
}

TEST_F(DocumentLoaderImplTest, DocumentSize) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(123456789);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_EQ(static_cast<int>(loader.GetDocumentSize()),
            client.full_page_loader_data()->content_length());
}

TEST_F(DocumentLoaderImplTest, DocumentSizeNoContentLength) {
  TestClient client;
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_EQ(0ul, loader.GetDocumentSize());
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  client.full_page_loader_data()->CallReadCallback(1000);
  client.full_page_loader_data()->CallReadCallback(500);
  client.full_page_loader_data()->CallReadCallback(0);
  EXPECT_EQ(kDefaultRequestSize + 1000ul + 500ul, loader.GetDocumentSize());
  EXPECT_TRUE(loader.IsDocumentComplete());
}

TEST_F(DocumentLoaderImplTest, ClearPendingRequests) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 100 +
                                                     58383);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(17 * kDefaultRequestSize + 100, 10);
  loader.ClearPendingRequests();
  loader.RequestData(15 * kDefaultRequestSize + 200, 20);
  // pending requests are accumulating, and will be processed after initial data
  // load.
  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    constexpr gfx::Range range_requested(15 * kDefaultRequestSize,
                                         16 * kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  // clear requests before Open callback.
  loader.ClearPendingRequests();
  // Current request should continue loading.
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->CallOpenCallback(0);
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(client.partial_loader_data()->closed());
  // Current request should continue loading, because no other request queued.

  loader.RequestData(18 * kDefaultRequestSize + 200, 20);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  // New request within close distance from the one currently loading. Loading
  // isn't restarted.
  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());

  loader.ClearPendingRequests();
  // request again two.
  loader.RequestData(60 * kDefaultRequestSize + 100, 10);
  loader.RequestData(35 * kDefaultRequestSize + 200, 20);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  {
    // new requset not with in close distance from current loading.
    // Loading should be restarted.
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    // The first requested chunk should be processed.
    constexpr gfx::Range range_requested(35 * kDefaultRequestSize,
                                         36 * kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->CallOpenCallback(0);
  // Override pending requests.
  loader.ClearPendingRequests();
  loader.RequestData(70 * kDefaultRequestSize + 100, 10);

  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  {
    // New requset not with in close distance from current loading.
    // Loading should be restarted .
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    // The first requested chunk should be processed.
    constexpr gfx::Range range_requested(70 * kDefaultRequestSize,
                                         71 * kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
}

TEST_F(DocumentLoaderImplTest, GetBlock) {
  std::vector<char> buffer(kDefaultRequestSize);
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20 +
                                                     58383);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(loader.GetBlock(0, 1000, buffer.data()));
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(loader.GetBlock(0, 1000, buffer.data()));
  EXPECT_FALSE(loader.GetBlock(kDefaultRequestSize, 1500, buffer.data()));
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(loader.GetBlock(kDefaultRequestSize, 1500, buffer.data()));

  EXPECT_FALSE(loader.GetBlock(17 * kDefaultRequestSize, 3000, buffer.data()));
  loader.RequestData(17 * kDefaultRequestSize + 100, 10);
  EXPECT_FALSE(loader.GetBlock(17 * kDefaultRequestSize, 3000, buffer.data()));

  // Requests queue is processed only on receiving data.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  client.SendAllPartialData();
  EXPECT_TRUE(loader.GetBlock(17 * kDefaultRequestSize, 3000, buffer.data()));
}

TEST_F(DocumentLoaderImplTest, IsDataAvailable) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20 +
                                                     58383);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  EXPECT_FALSE(loader.IsDataAvailable(0, 1000));
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(loader.IsDataAvailable(0, 1000));
  EXPECT_FALSE(loader.IsDataAvailable(kDefaultRequestSize, 1500));
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(loader.IsDataAvailable(kDefaultRequestSize, 1500));

  EXPECT_FALSE(loader.IsDataAvailable(17 * kDefaultRequestSize, 3000));
  loader.RequestData(17 * kDefaultRequestSize + 100, 10);
  EXPECT_FALSE(loader.IsDataAvailable(17 * kDefaultRequestSize, 3000));

  // Requests queue is processed only on receiving data.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  client.SendAllPartialData();
  EXPECT_TRUE(loader.IsDataAvailable(17 * kDefaultRequestSize, 3000));
}

TEST_F(DocumentLoaderImplTest, RequestData) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 100 +
                                                     58383);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(37 * kDefaultRequestSize + 200, 10);
  loader.RequestData(25 * kDefaultRequestSize + 600, 100);
  loader.RequestData(13 * kDefaultRequestSize + 900, 500);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    constexpr gfx::Range range_requested(13 * kDefaultRequestSize,
                                         14 * kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  client.partial_loader_data()->CallOpenCallback(0);
  // Override pending requests.
  loader.ClearPendingRequests();
  loader.RequestData(38 * kDefaultRequestSize + 200, 10);
  loader.RequestData(26 * kDefaultRequestSize + 600, 100);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    constexpr gfx::Range range_requested(26 * kDefaultRequestSize,
                                         27 * kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  client.partial_loader_data()->CallOpenCallback(0);
  // Override pending requests.
  loader.ClearPendingRequests();
  loader.RequestData(39 * kDefaultRequestSize + 200, 10);
  // Requests queue is processed only on receiving data.
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  {
    EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
    constexpr gfx::Range range_requested(39 * kDefaultRequestSize,
                                         40 * kDefaultRequestSize);
    EXPECT_EQ(range_requested.start(),
              client.partial_loader_data()->open_byte_range().start());
    EXPECT_LE(range_requested.end(),
              client.partial_loader_data()->open_byte_range().end());
    client.partial_loader_data()->set_byte_range(
        client.partial_loader_data()->open_byte_range());
  }
  // Fill all gaps.
  while (!loader.IsDocumentComplete()) {
    client.SendAllPartialData();
  }
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, DoNotLoadAvailablePartialData) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20 +
                                                     58383);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  // Send more data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  loader.RequestData(2 * kDefaultRequestSize + 200, 10);

  // Send more data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  // Partial loading should not have started for already available data.
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, DoNotLoadDataAfterComplete) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  for (int i = 0; i < 20; ++i) {
    client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  }

  EXPECT_TRUE(loader.IsDocumentComplete());

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  EXPECT_TRUE(client.partial_loader_data()->closed());
  EXPECT_TRUE(client.full_page_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, DoNotLoadPartialDataAboveDocumentSize) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(20 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, MergePendingRequests) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 50 +
                                                     58383);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  loader.RequestData(17 * kDefaultRequestSize + 200, 10);
  loader.RequestData(16 * kDefaultRequestSize + 600, 100);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  constexpr gfx::Range range_requested(16 * kDefaultRequestSize,
                                       18 * kDefaultRequestSize);
  EXPECT_EQ(range_requested.start(),
            client.partial_loader_data()->open_byte_range().start());
  EXPECT_LE(range_requested.end(),
            client.partial_loader_data()->open_byte_range().end());

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());

  // Fill all gaps.
  while (!loader.IsDocumentComplete()) {
    client.SendAllPartialData();
  }
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, PartialStopOnStatusCodeError) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_status_code(404);
  client.partial_loader_data()->CallOpenCallback(0);
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest,
       PartialAsFullDocumentLoadingRangeRequestNoRangeField) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_byte_range(gfx::Range::InvalidRange());
  client.partial_loader_data()->CallOpenCallback(0);
  EXPECT_FALSE(client.partial_loader_data()->closed());
  // Partial loader is used to load the whole page, like full page loader.
  EXPECT_FALSE(loader.is_partial_loader_active());
}

TEST_F(DocumentLoaderImplTest, PartialMultiPart) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_is_multipart(true);
  client.partial_loader_data()->CallOpenCallback(0);
  client.partial_loader_data()->set_byte_range(
      gfx::Range(17 * kDefaultRequestSize, 18 * kDefaultRequestSize));
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(
      loader.IsDataAvailable(17 * kDefaultRequestSize, kDefaultRequestSize));
}

TEST_F(DocumentLoaderImplTest, PartialMultiPartRangeError) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_is_multipart(true);
  client.partial_loader_data()->CallOpenCallback(0);
  client.partial_loader_data()->set_byte_range(gfx::Range::InvalidRange());
  client.partial_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_FALSE(
      loader.IsDataAvailable(17 * kDefaultRequestSize, kDefaultRequestSize));
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, PartialConnectionErrorOnOpen) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->CallOpenCallback(-3);
  EXPECT_TRUE(client.partial_loader_data()->closed());

  // Partial loading should not restart after any error.
  loader.RequestData(18 * kDefaultRequestSize + 200, 10);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, PartialConnectionErrorOnRead) {
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(17 * kDefaultRequestSize + 200, 10);

  // Send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  EXPECT_TRUE(client.partial_loader_data()->IsWaitOpen());
  client.partial_loader_data()->set_byte_range(
      gfx::Range(17 * kDefaultRequestSize, 18 * kDefaultRequestSize));
  client.partial_loader_data()->CallOpenCallback(0);
  EXPECT_TRUE(client.partial_loader_data()->IsWaitRead());
  client.partial_loader_data()->CallReadCallback(-3);
  EXPECT_TRUE(client.partial_loader_data()->closed());

  // Partial loading should not restart after any error.
  loader.RequestData(18 * kDefaultRequestSize + 200, 10);

  EXPECT_FALSE(client.partial_loader_data()->IsWaitOpen());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, ClientCompleteCallbacks) {
  MockClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  for (int i = 0; i < 19; ++i)
    client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnDocumentComplete()).Times(1);
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest, ClientCompleteCallbacksNoContentLength) {
  MockClient client;
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnDocumentCanceled()).Times(0);
  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  for (int i = 0; i < 20; ++i)
    client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnDocumentCanceled()).Times(0);
  EXPECT_CALL(client, OnDocumentComplete()).Times(1);
  client.full_page_loader_data()->CallReadCallback(0);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest, ClientCancelCallback) {
  MockClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnDocumentCanceled()).Times(0);
  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  for (int i = 0; i < 10; ++i)
    client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnDocumentComplete()).Times(0);
  EXPECT_CALL(client, OnDocumentCanceled()).Times(1);
  client.full_page_loader_data()->CallReadCallback(-3);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest, NewDataAvailable) {
  MockClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDefaultRequestSize * 20);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnNewDataReceived()).Times(1);
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnNewDataReceived()).Times(1);
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize - 100);
  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnNewDataReceived()).Times(1);
  client.full_page_loader_data()->CallReadCallback(100);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest, ClientPendingRequestCompleteFullLoader) {
  MockClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  loader.RequestData(1000, 4000);

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest, ClientPendingRequestCompletePartialLoader) {
  MockClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  loader.RequestData(15 * kDefaultRequestSize + 4000, 4000);

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  client.SendAllPartialData();
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest,
       ClientPendingRequestCompletePartialAndFullLoader) {
  MockClient client;
  client.SetCanUsePartialLoading();
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  loader.RequestData(16 * kDefaultRequestSize + 4000, 4000);
  loader.RequestData(4 * kDefaultRequestSize + 4000, 4000);

  for (int i = 0; i < 5; ++i)
    client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);

  Mock::VerifyAndClear(&client);

  EXPECT_CALL(client, OnPendingRequestComplete()).Times(1);
  client.SendAllPartialData();
  Mock::VerifyAndClear(&client);
}

TEST_F(DocumentLoaderImplTest, IgnoreDataMoreThanExpectedWithPartial) {
  static constexpr uint32_t kDocSize = kDefaultRequestSize * 80 - 321;
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDocSize);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  // Request data at and.
  loader.RequestData(kDocSize - 100, 100);

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(client.partial_loader_data()->closed());

  // Request data at middle to continue loading partial, but not all remaining
  // data.
  loader.RequestData(kDocSize / 2, 100);

  // Fill data at the end, the partial loding should be started for second
  // requested data after receive data for first request.
  client.SendAllPartialData();

  ASSERT_TRUE(client.partial_loader_data()->IsWaitOpen());
  // Process second request.
  const uint32_t expected_length =
      client.partial_loader_data()->open_byte_range().length();

  // Send data.
  client.partial_loader_data()->set_byte_range(
      client.partial_loader_data()->open_byte_range());
  client.partial_loader_data()->CallOpenCallback(0);
  uint32_t length = expected_length;
  while (length > 0) {
    constexpr uint32_t max_part_len = kDefaultRequestSize;
    const uint32_t part_len = std::min(length, max_part_len);
    client.partial_loader_data()->CallReadCallback(part_len);
    length -= part_len;
  }

  // The partial loading should be finished for current chunks sequence, if
  // expected range was received, and remaining sequence should start loading.
  EXPECT_FALSE(client.partial_loader_data()->IsWaitRead());
  ASSERT_TRUE(client.partial_loader_data()->IsWaitOpen());

  // Send other document data.
  client.SendAllPartialData();
  // The downloads should be finished.
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

TEST_F(DocumentLoaderImplTest, IgnoreDataMoreThanExpectedWithPartialAtFileEnd) {
  static constexpr uint32_t kExtraSize = 100;
  static constexpr uint32_t kRealSize = kDefaultRequestSize * 20 - 300;
  static constexpr uint32_t kDocSize = kRealSize - kExtraSize;
  TestClient client;
  client.SetCanUsePartialLoading();
  client.full_page_loader_data()->set_content_length(kDocSize);
  DocumentLoaderImpl loader(&client);
  loader.Init(client.CreateFullPageLoader(), "http://url.com");
  // Request data at middle.
  static constexpr uint32_t kFirstPartial = kDefaultRequestSize * 11;
  loader.RequestData(kFirstPartial, kDefaultRequestSize);

  // Always send initial data from FullPageLoader.
  client.full_page_loader_data()->CallReadCallback(kDefaultRequestSize);
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_FALSE(client.partial_loader_data()->closed());

  // Send data to file end and extra non expected data.
  client.partial_loader_data()->set_byte_range(
      gfx::Range(kFirstPartial, kRealSize));
  client.partial_loader_data()->CallOpenCallback(0);
  uint32_t length = client.partial_loader_data()->byte_range().length();
  while (length > 0) {
    constexpr uint32_t max_part_len = kDefaultRequestSize;
    const uint32_t part_len = std::min(length, max_part_len);
    client.partial_loader_data()->CallReadCallback(part_len);
    length -= part_len;
  }

  // The partial loading should be finished for current chunks sequence, if
  // eof was reached, and remaining sequence should start loading.
  EXPECT_FALSE(client.partial_loader_data()->IsWaitRead());
  EXPECT_EQ(gfx::Range(kDefaultRequestSize, kFirstPartial),
            client.partial_loader_data()->open_byte_range());

  // Send other document data.
  client.SendAllPartialData();
  // The downloads should be finished.
  EXPECT_TRUE(client.full_page_loader_data()->closed());
  EXPECT_TRUE(client.partial_loader_data()->closed());
}

}  // namespace chrome_pdf
