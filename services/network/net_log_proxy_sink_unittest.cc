// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/net_log_proxy_sink.h"

#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "net/log/test_net_log.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeNetLogProxySource : public network::mojom::NetLogProxySource {
 public:
  explicit FakeNetLogProxySource(
      mojo::PendingReceiver<network::mojom::NetLogProxySource>
          proxy_source_receiver)
      : proxy_source_receiver_(this, std::move(proxy_source_receiver)),
        run_loop_(std::make_unique<base::RunLoop>()) {}
  FakeNetLogProxySource(const FakeNetLogProxySource&) = delete;
  FakeNetLogProxySource& operator=(const FakeNetLogProxySource&) = delete;

  // mojom::NetLogProxySource:
  void UpdateCaptureModes(net::NetLogCaptureModeSet modes) override {
    EXPECT_FALSE(already_stored_last_modes_);
    already_stored_last_modes_ = true;
    last_modes_ = modes;

    run_loop_->Quit();
  }

  net::NetLogCaptureModeSet WaitForUpdateCaptureModes() {
    run_loop_->Run();
    EXPECT_TRUE(already_stored_last_modes_);
    already_stored_last_modes_ = false;
    net::NetLogCaptureModeSet last_modes = last_modes_;
    last_modes_ = 0;
    run_loop_ = std::make_unique<base::RunLoop>();
    return last_modes;
  }

 private:
  mojo::Receiver<network::mojom::NetLogProxySource> proxy_source_receiver_;
  bool already_stored_last_modes_ = false;
  std::unique_ptr<base::RunLoop> run_loop_;
  net::NetLogCaptureModeSet last_modes_;
};

void CreateObserver(std::unique_ptr<net::RecordingNetLogObserver>* out_observer,
                    net::NetLogCaptureMode capture_mode) {
  *out_observer = std::make_unique<net::RecordingNetLogObserver>(capture_mode);
}

void DestroyObserver(
    std::unique_ptr<net::RecordingNetLogObserver>* out_observer) {
  out_observer->reset();
}

}  // namespace

TEST(NetLogProxySink, TestMultipleObservers) {
  base::test::TaskEnvironment scoped_task_environment;

  network::NetLogProxySink net_log_sink;

  mojo::PendingRemote<network::mojom::NetLogProxySource> proxy_source_remote;
  mojo::PendingReceiver<network::mojom::NetLogProxySource>
      proxy_source_receiver =
          proxy_source_remote.InitWithNewPipeAndPassReceiver();
  mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote;

  FakeNetLogProxySource net_log_proxy_source(std::move(proxy_source_receiver));

  net_log_sink.AttachSource(std::move(proxy_source_remote),
                            proxy_sink_remote.BindNewPipeAndPassReceiver());

  // Attaching should notify with the current capture modeset.
  EXPECT_EQ(0U, net_log_proxy_source.WaitForUpdateCaptureModes());

  std::unique_ptr<net::RecordingNetLogObserver> net_log_observer1 =
      std::make_unique<net::RecordingNetLogObserver>(
          net::NetLogCaptureMode::kIncludeSensitive);
  EXPECT_EQ(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kIncludeSensitive),
      net_log_proxy_source.WaitForUpdateCaptureModes());

  std::unique_ptr<net::RecordingNetLogObserver> net_log_observer2;
  {
    // Create observer2 on a different thread.
    base::RunLoop run_loop;
    base::ThreadPool::PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&CreateObserver, &net_log_observer2,
                       net::NetLogCaptureMode::kDefault),
        run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_EQ(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kIncludeSensitive) |
          net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kDefault),
      net_log_proxy_source.WaitForUpdateCaptureModes());

  base::RunLoop add_entry_runloop;
  net_log_observer1->SetThreadsafeAddEntryCallback(
      add_entry_runloop.QuitClosure());

  base::TimeTicks source1_start_time =
      base::TimeTicks() + base::Milliseconds(10);
  base::TimeTicks source1_event0_time =
      source1_start_time + base::Milliseconds(1);
  auto source1_event0_params = base::Value::Dict().Set("hello", "world");
  proxy_sink_remote->AddEntry(
      static_cast<uint32_t>(net::NetLogEventType::REQUEST_ALIVE),
      net::NetLogSource(net::NetLogSourceType::URL_REQUEST, 1U,
                        source1_start_time),
      net::NetLogEventPhase::BEGIN, source1_event0_time,
      source1_event0_params.Clone());

  add_entry_runloop.Run();

  auto entries = net_log_observer1->GetEntries();
  ASSERT_EQ(1U, entries.size());

  EXPECT_EQ(net::NetLogEventType::REQUEST_ALIVE, entries[0].type);
  EXPECT_EQ(net::NetLogSourceType::URL_REQUEST, entries[0].source.type);
  EXPECT_EQ(1U, entries[0].source.id);
  EXPECT_EQ(source1_start_time, entries[0].source.start_time);
  EXPECT_EQ(net::NetLogEventPhase::BEGIN, entries[0].phase);
  EXPECT_EQ(source1_event0_time, entries[0].time);
  EXPECT_TRUE(entries[0].HasParams());
  EXPECT_EQ(source1_event0_params, entries[0].params);

  auto entries2 = net_log_observer2->GetEntries();
  ASSERT_EQ(1U, entries2.size());
  EXPECT_EQ(entries[0].type, entries2[0].type);
  EXPECT_EQ(entries[0].source.type, entries2[0].source.type);
  EXPECT_EQ(entries[0].source.id, entries2[0].source.id);
  EXPECT_EQ(entries[0].source.start_time, entries2[0].source.start_time);
  EXPECT_EQ(entries[0].phase, entries2[0].phase);
  EXPECT_EQ(entries[0].time, entries2[0].time);
  EXPECT_EQ(entries[0].params, entries2[0].params);

  net_log_observer1.reset();
  EXPECT_EQ(net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kDefault),
            net_log_proxy_source.WaitForUpdateCaptureModes());

  // Destroy observer2 on a different thread.
  base::ThreadPool::PostTask(
      FROM_HERE, base::BindOnce(DestroyObserver, &net_log_observer2));
  EXPECT_EQ(0U, net_log_proxy_source.WaitForUpdateCaptureModes());
}

TEST(NetLogProxySink, TestAttachAfterObserverAdded) {
  base::test::TaskEnvironment scoped_task_environment;

  // Start observing before creating/attaching the net log proxy.
  std::unique_ptr<net::RecordingNetLogObserver> net_log_observer1 =
      std::make_unique<net::RecordingNetLogObserver>(
          net::NetLogCaptureMode::kIncludeSensitive);

  network::NetLogProxySink net_log_sink;

  mojo::PendingRemote<network::mojom::NetLogProxySource> proxy_source_remote;
  mojo::PendingReceiver<network::mojom::NetLogProxySource>
      proxy_source_receiver =
          proxy_source_remote.InitWithNewPipeAndPassReceiver();
  mojo::Remote<network::mojom::NetLogProxySink> proxy_sink_remote;

  FakeNetLogProxySource net_log_proxy_source(std::move(proxy_source_receiver));

  net_log_sink.AttachSource(std::move(proxy_source_remote),
                            proxy_sink_remote.BindNewPipeAndPassReceiver());

  // Attaching should notify with the current capture modeset.
  EXPECT_EQ(
      net::NetLogCaptureModeToBit(net::NetLogCaptureMode::kIncludeSensitive),
      net_log_proxy_source.WaitForUpdateCaptureModes());

  net_log_observer1.reset();
  EXPECT_EQ(0U, net_log_proxy_source.WaitForUpdateCaptureModes());
}
