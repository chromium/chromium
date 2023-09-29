// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "media/base/mock_media_log.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/media/buffered_data_source_host_impl.h"
#include "third_party/blink/renderer/platform/media/hls_data_source_provider_impl.h"
#include "third_party/blink/renderer/platform/media/multi_buffer_data_source.h"

namespace blink {

using base::test::RunOnceCallback;
using testing::_;
using testing::AtLeast;
using testing::ByMove;
using testing::DoAll;
using testing::Eq;
using testing::Invoke;
using testing::NiceMock;
using testing::NotNull;
using testing::Ref;
using testing::Return;
using testing::SaveArg;
using testing::SetArgPointee;
using testing::StrictMock;

namespace {

class TestUrlIndex : public UrlIndex {
 public:
  explicit TestUrlIndex(scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : UrlIndex(nullptr, task_runner) {}

  scoped_refptr<UrlData> NewUrlData(const GURL& url,
                                    UrlData::CorsMode cors_mode) override {
    NOTREACHED();
    return nullptr;
  }
};

class TestUrlData : public UrlData {
 public:
  TestUrlData(const GURL& url,
              UrlIndex* url_index,
              scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : UrlData(url, UrlData::CORS_UNSPECIFIED, url_index, task_runner) {}

  ResourceMultiBuffer* multibuffer() override { return nullptr; }
};

class MockBufferedDataSourceHost : public BufferedDataSourceHost {
 public:
  MockBufferedDataSourceHost() = default;
  MockBufferedDataSourceHost(const MockBufferedDataSourceHost&) = delete;
  MockBufferedDataSourceHost& operator=(const MockBufferedDataSourceHost&) =
      delete;
  ~MockBufferedDataSourceHost() override = default;

  MOCK_METHOD1(SetTotalBytes, void(int64_t total_bytes));
  MOCK_METHOD2(AddBufferedByteRange, void(int64_t start, int64_t end));
};

class MockMultiBufferDataSource : public MultiBufferDataSource {
 public:
  MockMultiBufferDataSource(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      scoped_refptr<UrlData> url_data,
      media::MediaLog* media_log,
      BufferedDataSourceHost* host,
      DownloadingCB downloading_cb)
      : MultiBufferDataSource(std::move(task_runner),
                              std::move(url_data),
                              media_log,
                              host,
                              std::move(downloading_cb)) {}

  void Initialize(InitializeCB init_cb) override {
    InitializeCalled();
    std::move(init_cb).Run(true);
  }

  MOCK_METHOD(void, InitializeCalled, (), ());
  MOCK_METHOD(void, Abort, (), (override));
  MOCK_METHOD(void,
              Read,
              (int64_t, int, uint8_t*, media::DataSource::ReadCB),
              (override));
};

}  // namespace

class HlsDataSourceProviderImplUnittest : public testing::Test {
 public:
  HlsDataSourceProviderImplUnittest()
      : media_log_(std::make_unique<NiceMock<media::MockMediaLog>>()),
        tick_clock_(std::make_unique<base::SimpleTestTickClock>()),
        mock_host_(std::make_unique<MockBufferedDataSourceHost>()) {
    url_index_ = std::make_unique<TestUrlIndex>(
        task_environment_.GetMainThreadTaskRunner());
  }

  void SetUpDSP() {
    impl_ = std::make_unique<HlsDataSourceProviderImpl>(
        media_log_.get(), url_index_.get(),
        task_environment_.GetMainThreadTaskRunner(),
        task_environment_.GetMainThreadTaskRunner(), tick_clock_.get());
  }

  ~HlsDataSourceProviderImplUnittest() override {
    data_source_.reset();
    // URL data needs to be freed before the url index, because UrlData keeps
    // a rawptr to UrlIndex.
    task_environment_.RunUntilIdle();
    url_index_.reset();
  }

  media::HlsDataSourceProvider::RequestCb StoreDSP() {
    return base::BindOnce(&HlsDataSourceProviderImplUnittest::StoreDSPImpl,
                          base::Unretained(this));
  }

  void StoreDSPImpl(std::unique_ptr<media::HlsDataSource> data_source) {
    ASSERT_EQ(data_source_, nullptr);
    data_source_ = std::move(data_source);
  }

  std::unique_ptr<MockMultiBufferDataSource> MakeMockDataSource() {
    return std::make_unique<MockMultiBufferDataSource>(
        task_environment_.GetMainThreadTaskRunner(),
        NewUrlData(GURL("https://example.com")), media_log_.get(),
        mock_host_.get(),
        base::BindRepeating(&HlsDataSourceProviderImplUnittest::Downloading,
                            base::Unretained(this)));
  }

  void Downloading(bool) {}

  scoped_refptr<UrlData> NewUrlData(const GURL& url) {
    return new TestUrlData(url, url_index_.get(),
                           task_environment_.GetMainThreadTaskRunner());
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<media::MediaLog> media_log_;
  std::unique_ptr<base::TickClock> tick_clock_;
  std::unique_ptr<MockBufferedDataSourceHost> mock_host_;
  std::unique_ptr<TestUrlIndex> url_index_;
  std::unique_ptr<HlsDataSourceProviderImpl> impl_;
  std::unique_ptr<media::HlsDataSource> data_source_;
};

TEST_F(HlsDataSourceProviderImplUnittest, TestMultibuffersCreateReadAbort) {
  SetUpDSP();
  std::unique_ptr<MockMultiBufferDataSource> mock_ds = MakeMockDataSource();

  EXPECT_CALL(*mock_ds, InitializeCalled());
  EXPECT_CALL(*mock_ds, Read(0, 50, nullptr, _));
  EXPECT_CALL(*mock_ds, Abort());

  impl_->RequestMockDataSourceForTesting(std::move(mock_ds), StoreDSP());
  task_environment_.RunUntilIdle();

  ASSERT_NE(data_source_, nullptr);
  data_source_->Read(
      0, 50, nullptr,
      base::BindOnce([](media::HlsDataSource::ReadStatus::Or<size_t>) {
        // This callback should never be executed because mock_ds::Read is
        // never replied to.
        FAIL() << "This Read() should never complete.";
      }));

  task_environment_.RunUntilIdle();
  // Resetting impl triggers the abort.
  impl_.reset();
  task_environment_.RunUntilIdle();
}

}  // namespace blink
