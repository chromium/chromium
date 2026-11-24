// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/demuxer_manager.h"

#include <memory>

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "media/base/mock_demuxer_host.h"
#include "media/base/mock_media_log.h"
#include "media/filters/hls_test_helpers.h"
#include "media/filters/mock_demuxer_manager_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace media {

namespace {

using ::base::test::RunClosure;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class ForwardingHlsDataSourceProvider : public HlsDataSourceProvider {
 public:
  explicit ForwardingHlsDataSourceProvider(HlsDataSourceProvider* target)
      : target_(target) {}

  void ReadFromUrl(UrlDataSegment segment, ReadCb callback) override {
    target_->ReadFromUrl(std::move(segment), std::move(callback));
  }

  void ReadFromExistingStream(std::unique_ptr<HlsDataSourceStream> stream,
                              ReadCb callback) override {
    target_->ReadFromExistingStream(std::move(stream), std::move(callback));
  }

  void AbortPendingReads(base::OnceClosure callback) override {
    target_->AbortPendingReads(std::move(callback));
  }

 private:
  raw_ptr<HlsDataSourceProvider> target_;
};

}  // namespace

TEST(DemuxerManagerTest, HlsFallbackPreservesPreRedirectLoadedUrlAndTaint) {
  base::test::TaskEnvironment task_environment;
  scoped_refptr<base::SequencedTaskRunner> media_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner({});
  NiceMock<MockMediaLog> media_log;
  NiceMock<MockDemuxerHost> host;
  NiceMock<MockDemuxerManagerClient> client;
  MockHlsDataSourceProvider mock_dsp;

  const GURL pre_redirect_url("https://victim.example/api/watch?id=1");
  const GURL post_redirect_url(
      "https://cdn.example/hls/stream_endpoint?cap_token=secret");

  auto manager = std::make_unique<DemuxerManager>(
      &client, url::Origin::Create(GURL("https://attacker.example")),
      media_task_runner, &media_log, /*demuxer_override=*/nullptr);
  manager->SetLoadedUrl(pre_redirect_url);

  auto data_source = std::make_unique<NiceMock<MockDataSource>>();
  ON_CALL(*data_source, GetUrlAfterRedirects())
      .WillByDefault(Return(post_redirect_url));
  ON_CALL(*data_source, WouldTaintOrigin()).WillByDefault(Return(true));
  EXPECT_CALL(*data_source, Stop());
  manager->SetDataSource(std::move(data_source));

  base::RunLoop restart_loop;
  EXPECT_CALL(client, StopForDemuxerReset());
  EXPECT_CALL(client, RestartForDemuxerChange(true))
      .WillOnce(RunClosure(restart_loop.QuitClosure()));

  manager->OnPipelineError(DEMUXER_ERROR_DETECTED_HLS);
  EXPECT_TRUE(manager->WouldTaintOrigin());
  restart_loop.Run();

  EXPECT_EQ(manager->LoadedUrl(), pre_redirect_url);
  EXPECT_TRUE(manager->WouldTaintOrigin());

  EXPECT_CALL(client, GetHlsDataSourceProvider()).WillOnce([&]() {
    return base::SequenceBound<ForwardingHlsDataSourceProvider>(
        task_environment.GetMainThreadTaskRunner(), &mock_dsp);
  });

  Demuxer* created_demuxer = nullptr;
  PipelineStatus create_status = manager->CreateDemuxer(
      /*load_media_source=*/false, DataSource::METADATA,
      /*needs_first_frame=*/false,
      base::BindLambdaForTesting(
          [&](Demuxer* demuxer, Pipeline::StartType, bool, bool) {
            created_demuxer = demuxer;
            return PipelineStatus(PIPELINE_OK);
          }));
  ASSERT_EQ(create_status, PIPELINE_OK);
  ASSERT_NE(created_demuxer, nullptr);
  EXPECT_EQ(manager->GetDemuxerType(), DemuxerType::kManifestDemuxer);
  EXPECT_TRUE(manager->WouldTaintOrigin());

  EXPECT_CALL(mock_dsp, ReadFromUrl(_, _))
      .WillOnce([&](HlsDataSourceProvider::UrlDataSegment segment,
                    HlsDataSourceProvider::ReadCb callback) {
        EXPECT_EQ(segment.uri, pre_redirect_url);
        std::move(callback).Run(
            base::unexpected(HlsDemuxerStatus::Codes::kNetworkReadStopped));
      });

  base::test::TestFuture<PipelineStatus> init_status;
  media_task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&Demuxer::Initialize, base::Unretained(created_demuxer),
                     &host, init_status.GetSequenceBoundCallback()));
  EXPECT_EQ(init_status.Get(), DEMUXER_ERROR_COULD_NOT_PARSE);

  base::test::TestFuture<void> stopped;
  media_task_runner->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&Demuxer::Stop, base::Unretained(created_demuxer)),
      stopped.GetCallback());
  ASSERT_TRUE(stopped.Wait());

  manager.reset();
  base::test::TestFuture<void> deleted;
  media_task_runner->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                      deleted.GetCallback());
  ASSERT_TRUE(deleted.Wait());
}

}  // namespace media
