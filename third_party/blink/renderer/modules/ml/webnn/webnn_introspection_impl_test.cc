// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/ml/webnn/webnn_introspection_impl.h"

#include "services/webnn/public/cpp/webnn_buildflags.h"

#if BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webnn/webnn_introspection.mojom-blink.h"  // nogncheck
#include "third_party/blink/renderer/platform/scheduler/public/main_thread.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {
namespace {

using testing::_;
using testing::Invoke;

class MockWebNNIntrospectionClient
    : public blink::mojom::blink::WebNNIntrospectionClient {
 public:
  MockWebNNIntrospectionClient() : receiver_(this) {}

  mojo::PendingRemote<blink::mojom::blink::WebNNIntrospectionClient>
  BindNewPipeAndPassRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void,
              OnGraphRecorded,
              (::mojo_base::BigBuffer json_data),
              (override));

 private:
  mojo::Receiver<blink::mojom::blink::WebNNIntrospectionClient> receiver_;
};

class WebNNIntrospectionImplTest : public testing::Test {
 public:
  WebNNIntrospectionImplTest() = default;
  ~WebNNIntrospectionImplTest() override = default;

 protected:
  test::TaskEnvironment task_environment_;
};

TEST_F(WebNNIntrospectionImplTest, OnGraphRecordedFromMainAndWorkerThreads) {
  MockWebNNIntrospectionClient mock_client;
  WebNNIntrospectionImpl::GetInstance().SetClient(
      mock_client.BindNewPipeAndPassRemote());

  // Wait for Mojo connection
  EXPECT_TRUE(base::test::RunUntil([]() {
    return WebNNIntrospectionImpl::GetInstance().IsGraphRecordingEnabled();
  }));

  int call_count = 0;
  EXPECT_CALL(mock_client, OnGraphRecorded(_))
      .Times(2)
      .WillRepeatedly([&](::mojo_base::BigBuffer data) { call_count++; });

  // Call directly on main thread
  mojo_base::BigBuffer buffer1;
  WebNNIntrospectionImpl::GetInstance().OnGraphRecorded(std::move(buffer1));

  // Simulate call from a worker thread (background thread)
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindLambdaForTesting([]() {
        mojo_base::BigBuffer buffer2;
        WebNNIntrospectionImpl::GetInstance().OnGraphRecorded(
            std::move(buffer2));
      }));

  // Wait until both calls to OnGraphRecorded are received.
  EXPECT_TRUE(base::test::RunUntil([&]() { return call_count == 2; }));
}

}  // namespace
}  // namespace blink

#endif  // BUILDFLAG(WEBNN_ENABLE_GRAPH_DUMP)
