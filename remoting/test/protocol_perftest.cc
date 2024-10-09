// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <numeric>
#include <utility>

#include "base/base64.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/message_loop/message_pump_type.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/webrtc/thread_wrapper.h"
#include "net/base/network_change_notifier.h"
#include "net/test/test_data_directory.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/local_session_policies_provider.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/client/audio/audio_player.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/software_video_renderer.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/fake_desktop_environment.h"
#include "remoting/protocol/auth_util.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/frame_consumer.h"
#include "remoting/protocol/frame_stats.h"
#include "remoting/protocol/host_authentication_config.h"
#include "remoting/protocol/ice_config_fetcher.h"
#include "remoting/protocol/jingle_session_manager.h"
#include "remoting/protocol/me2me_host_authenticator_factory.h"
#include "remoting/protocol/session_config.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/protocol/video_frame_pump.h"
#include "remoting/protocol/video_renderer.h"
#include "remoting/signaling/fake_signal_strategy.h"
#include "remoting/test/cyclic_frame_generator.h"
#include "remoting/test/fake_network_dispatcher.h"
#include "remoting/test/fake_port_allocator.h"
#include "remoting/test/fake_socket_factory.h"
#include "remoting/test/scroll_frame_generator.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

using base::test::TaskEnvironment;
using protocol::ChannelConfig;

namespace {

const char kHostJid[] = "host_jid@example.com/host";
const char kHostOwner[] = "jane.doe@example.com";
const char kClientJid[] = "jane.doe@example.com/client";
const char kHostId[] = "ABC123";
const char kHostPin[] = "123456";

struct NetworkPerformanceParams {
  // |buffer_s| defines buffer size in seconds. actual buffer size is calculated
  // based on bandwidth_kbps
  NetworkPerformanceParams(int bandwidth_kbps,
                           double buffer_s,
                           double latency_average_ms,
                           double latency_stddev_ms,
                           double out_of_order_rate,
                           double signaling_latency_ms)
      : bandwidth_kbps(bandwidth_kbps),
        max_buffers(buffer_s * bandwidth_kbps * 1000 / 8),
        latency_average(base::Milliseconds(latency_average_ms)),
        latency_stddev(base::Milliseconds(latency_stddev_ms)),
        out_of_order_rate(out_of_order_rate),
        signaling_latency(base::Milliseconds(signaling_latency_ms)) {}

  int bandwidth_kbps;
  int max_buffers;
  base::TimeDelta latency_average;
  base::TimeDelta latency_stddev;
  double out_of_order_rate;
  base::TimeDelta signaling_latency;
};

class FakeCursorShapeStub : public protocol::CursorShapeStub {
 public:
  FakeCursorShapeStub() = default;
  ~FakeCursorShapeStub() override = default;

  // protocol::CursorShapeStub interface.
  void SetCursorShape(const protocol::CursorShapeInfo& cursor_shape) override {}
};

// Stub used for Me2MeHostAuthenticatorFactory::CheckAccessPermissionCallback.
bool CheckAccessPermission(std::string_view user_email) {
  return true;
}

}  // namespace

class ProtocolPerfTest
    : public testing::Test,
      public testing::WithParamInterface<NetworkPerformanceParams>,
      public ClientUserInterface,
      public protocol::FrameConsumer,
      public protocol::FrameStatsConsumer,
      public HostStatusObserver {
 public:
  ProtocolPerfTest()
      : task_environment_(TaskEnvironment::MainThreadType::IO),
        host_thread_("host"),
        capture_thread_("capture"),
        encode_thread_("encode"),
        decode_thread_("decode") {
    host_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    capture_thread_.Start();
    encode_thread_.Start();
    decode_thread_.Start();

    network_change_notifier_ = net::NetworkChangeNotifier::CreateIfNeeded();

    desktop_environment_factory_ =
        std::make_unique<FakeDesktopEnvironmentFactory>(
            capture_thread_.task_runner());
  }

  ProtocolPerfTest(const ProtocolPerfTest&) = delete;
  ProtocolPerfTest& operator=(const ProtocolPerfTest&) = delete;

  virtual ~ProtocolPerfTest() {
    host_thread_.task_runner()->DeleteSoon(FROM_HERE, host_.release());
    host_thread_.task_runner()->DeleteSoon(FROM_HERE,
                                           host_signaling_.release());
    base::RunLoop().RunUntilIdle();
  }

  // ClientUserInterface interface.
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error) override {
    if (state == protocol::ConnectionToHost::CONNECTED) {
      client_connected_ = true;
      if (host_connected_) {
        connecting_loop_->Quit();
      }
    }
  }
  void OnConnectionReady(bool ready) override {}
  void OnRouteChanged(const std::string& channel_name,
                      const protocol::TransportRoute& route) override {}
  void SetCapabilities(const std::string& capabilities) override {}
  void SetPairingResponse(
      const protocol::PairingResponse& pairing_response) override {}
  void DeliverHostMessage(const protocol::ExtensionMessage& message) override {}
  void SetDesktopSize(const webrtc::DesktopSize& size,
                      const webrtc::DesktopVector& dpi) override {}
  protocol::ClipboardStub* GetClipboardStub() override { return nullptr; }
  protocol::CursorShapeStub* GetCursorShapeStub() override {
    return &cursor_shape_stub_;
  }
  protocol::KeyboardLayoutStub* GetKeyboardLayoutStub() override {
    return nullptr;
  }

  // protocol::FrameConsumer interface.
  std::unique_ptr<webrtc::DesktopFrame> AllocateFrame(
      const webrtc::DesktopSize& size) override {
    return std::make_unique<webrtc::BasicDesktopFrame>(size);
  }

  void DrawFrame(std::unique_ptr<webrtc::DesktopFrame> frame,
                 base::OnceClosure done) override {
    last_video_frame_ = std::move(frame);
    if (on_frame_task_) {
      on_frame_task_.Run();
    }
    if (done) {
      std::move(done).Run();
    }
  }

  protocol::FrameConsumer::PixelFormat GetPixelFormat() override {
    return FORMAT_BGRA;
  }

  // FrameStatsConsumer interface.
  void OnVideoFrameStats(const protocol::FrameStats& frame_stats) override {
    // Ignore store stats for empty frames.
    if (!frame_stats.host_stats.frame_size) {
      return;
    }

    frame_stats_.push_back(frame_stats);

    if (waiting_frame_stats_loop_ &&
        frame_stats_.size() >= num_expected_frame_stats_) {
      waiting_frame_stats_loop_->Quit();
    }
  }

  // HostStatusObserver interface.
  void OnClientAuthenticated(const std::string& jid) override {
    if (event_timestamp_source_) {
      auto& session = host_->client_sessions_for_tests().front();
      session->SetEventTimestampsSourceForTests(
          std::move(event_timestamp_source_));
    }
  }

  void OnClientConnected(const std::string& jid) override {
    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProtocolPerfTest::OnHostConnectedMainThread,
                                  base::Unretained(this)));
  }

 protected:
  void WaitConnected() {
    client_connected_ = false;
    host_connected_ = false;

    connecting_loop_ = std::make_unique<base::RunLoop>();
    connecting_loop_->Run();

    ASSERT_TRUE(client_connected_ && host_connected_);
  }

  void OnHostConnectedMainThread() {
    host_connected_ = true;
    if (client_connected_) {
      connecting_loop_->Quit();
    }
  }

  std::unique_ptr<webrtc::DesktopFrame> ReceiveFrame() {
    last_video_frame_.reset();

    waiting_frames_loop_ = std::make_unique<base::RunLoop>();
    on_frame_task_ = waiting_frames_loop_->QuitClosure();
    waiting_frames_loop_->Run();
    waiting_frames_loop_.reset();

    EXPECT_TRUE(last_video_frame_);
    return std::move(last_video_frame_);
  }

  void WaitFrameStats(int num_frames) {
    num_expected_frame_stats_ = num_frames;

    waiting_frame_stats_loop_ = std::make_unique<base::RunLoop>();
    waiting_frame_stats_loop_->Run();
    waiting_frame_stats_loop_.reset();

    EXPECT_GE(frame_stats_.size(), num_expected_frame_stats_);
  }

  // Creates test host and client and starts connection between them. Caller
  // should call WaitConnected() to wait until connection is established. The
  // host is started on |host_thread_| while the client works on the main
  // thread.
  void StartHostAndClient(bool use_webrtc) {
    fake_network_dispatcher_ = new FakeNetworkDispatcher();

    client_signaling_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kClientJid));

    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();

    protocol_config_ = protocol::CandidateSessionConfig::CreateDefault();
    protocol_config_->DisableAudioChannel();
    protocol_config_->set_webrtc_supported(use_webrtc);
    protocol_config_->set_ice_supported(!use_webrtc);

    host_thread_.task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ProtocolPerfTest::StartHost, base::Unretained(this)));
  }

  void StartHost() {
    DCHECK(host_thread_.task_runner()->BelongsToCurrentThread());

    webrtc::ThreadWrapper::EnsureForCurrentMessageLoop();

    host_signaling_ =
        std::make_unique<FakeSignalStrategy>(SignalingAddress(kHostJid));
    host_signaling_->set_send_delay(GetParam().signaling_latency);
    host_signaling_->ConnectTo(client_signaling_.get());

    protocol::NetworkSettings network_settings(
        protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING);

    std::unique_ptr<FakePortAllocatorFactory> port_allocator_factory(
        new FakePortAllocatorFactory(fake_network_dispatcher_));
    port_allocator_factory->socket_factory()->SetBandwidth(
        GetParam().bandwidth_kbps * 1000 / 8, GetParam().max_buffers);
    port_allocator_factory->socket_factory()->SetLatency(
        GetParam().latency_average, GetParam().latency_stddev);
    port_allocator_factory->socket_factory()->set_out_of_order_rate(
        GetParam().out_of_order_rate);
    auto transport_context = base::MakeRefCounted<protocol::TransportContext>(
        std::move(port_allocator_factory),
        webrtc::ThreadWrapper::current()->SocketServer(),
        /*ice_config_fetcher=*/nullptr, protocol::TransportRole::SERVER);
    std::unique_ptr<protocol::SessionManager> session_manager(
        new protocol::JingleSessionManager(host_signaling_.get()));
    session_manager->set_protocol_config(protocol_config_->Clone());

    // Encoder runs on a separate thread, main thread is used for everything
    // else.
    host_ = std::make_unique<ChromotingHost>(
        desktop_environment_factory_.get(), std::move(session_manager),
        transport_context, host_thread_.task_runner(),
        encode_thread_.task_runner(),
        DesktopEnvironmentOptions::CreateDefault(),
        &local_session_policies_provider_);

    base::FilePath certs_dir(net::GetTestCertsDirectory());

    std::string host_cert;
    ASSERT_TRUE(base::ReadFileToString(
        certs_dir.AppendASCII("unittest.selfsigned.der"), &host_cert));

    base::FilePath key_path = certs_dir.AppendASCII("unittest.key.bin");
    std::string key_string;
    ASSERT_TRUE(base::ReadFileToString(key_path, &key_string));
    std::string key_base64 = base::Base64Encode(key_string);
    scoped_refptr<RsaKeyPair> key_pair = RsaKeyPair::FromString(key_base64);
    ASSERT_TRUE(key_pair.get());

    std::string host_pin_hash =
        protocol::GetSharedSecretHash(kHostId, kHostPin);
    auto auth_config = std::make_unique<protocol::HostAuthenticationConfig>(
        host_cert, key_pair);
    auth_config->AddSharedSecretAuth(host_pin_hash);
    auto auth_factory =
        std::make_unique<protocol::Me2MeHostAuthenticatorFactory>(
            base::BindRepeating(&CheckAccessPermission),
            std::move(auth_config));
    host_->SetAuthenticatorFactory(std::move(auth_factory));

    host_->status_monitor()->AddStatusObserver(this);
    host_->Start(kHostOwner);

    task_environment_.GetMainThreadTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(&ProtocolPerfTest::StartClientAfterHost,
                                  base::Unretained(this)));
  }

  void StartClientAfterHost() {
    client_signaling_->set_send_delay(GetParam().signaling_latency);
    client_signaling_->ConnectTo(host_signaling_.get());

    protocol::NetworkSettings network_settings(
        protocol::NetworkSettings::NAT_TRAVERSAL_OUTGOING);

    // Initialize client.
    client_context_ = std::make_unique<ClientContext>(
        base::SingleThreadTaskRunner::GetCurrentDefault());
    client_context_->Start();

    std::unique_ptr<FakePortAllocatorFactory> port_allocator_factory(
        new FakePortAllocatorFactory(fake_network_dispatcher_));
    client_socket_factory_ = port_allocator_factory->socket_factory();
    port_allocator_factory->socket_factory()->SetBandwidth(
        GetParam().bandwidth_kbps * 1000 / 8, GetParam().max_buffers);
    port_allocator_factory->socket_factory()->SetLatency(
        GetParam().latency_average, GetParam().latency_stddev);
    port_allocator_factory->socket_factory()->set_out_of_order_rate(
        GetParam().out_of_order_rate);
    auto transport_context = base::MakeRefCounted<protocol::TransportContext>(
        std::move(port_allocator_factory),
        webrtc::ThreadWrapper::current()->SocketServer(),
        /*ice_config_fetcher=*/nullptr, protocol::TransportRole::CLIENT);

    protocol::ClientAuthenticationConfig client_auth_config;
    client_auth_config.host_id = kHostId;
    client_auth_config.fetch_secret_callback = base::BindRepeating(
        &ProtocolPerfTest::FetchPin, base::Unretained(this));

    video_renderer_ = std::make_unique<SoftwareVideoRenderer>(this);
    video_renderer_->Initialize(*client_context_, this);

    client_ = std::make_unique<ChromotingClient>(
        client_context_.get(), this, video_renderer_.get(), nullptr);
    client_->set_protocol_config(protocol_config_->Clone());
    client_->Start(client_signaling_.get(), client_auth_config,
                   transport_context, kHostJid, std::string());
  }

  void FetchPin(
      bool pairing_supported,
      const protocol::SecretFetchedCallback& secret_fetched_callback) {
    secret_fetched_callback.Run(kHostPin);
  }

  void MeasureTotalLatency(bool use_webrtc);
  void MeasureScrollPerformance(bool use_webrtc);

  TaskEnvironment task_environment_;

  scoped_refptr<FakeNetworkDispatcher> fake_network_dispatcher_;

  base::Thread host_thread_;
  base::Thread capture_thread_;
  base::Thread encode_thread_;
  base::Thread decode_thread_;
  std::unique_ptr<FakeDesktopEnvironmentFactory> desktop_environment_factory_;

  scoped_refptr<protocol::InputEventTimestampsSource> event_timestamp_source_;

  FakeCursorShapeStub cursor_shape_stub_;
  LocalSessionPoliciesProvider local_session_policies_provider_;

  std::unique_ptr<protocol::CandidateSessionConfig> protocol_config_;

  std::unique_ptr<FakeSignalStrategy> host_signaling_;
  std::unique_ptr<FakeSignalStrategy> client_signaling_;

  std::unique_ptr<ChromotingHost> host_;
  std::unique_ptr<ClientContext> client_context_;
  std::unique_ptr<SoftwareVideoRenderer> video_renderer_;
  std::unique_ptr<ChromotingClient> client_;

  raw_ptr<FakePacketSocketFactory> client_socket_factory_;

  std::unique_ptr<base::RunLoop> connecting_loop_;
  std::unique_ptr<base::RunLoop> waiting_frames_loop_;

  std::unique_ptr<base::RunLoop> waiting_frame_stats_loop_;
  size_t num_expected_frame_stats_;

  bool client_connected_;
  bool host_connected_;

  base::RepeatingClosure on_frame_task_;

  std::unique_ptr<VideoPacket> last_video_packet_;
  std::unique_ptr<webrtc::DesktopFrame> last_video_frame_;
  std::vector<protocol::FrameStats> frame_stats_;

  std::unique_ptr<net::NetworkChangeNotifier> network_change_notifier_;
};

INSTANTIATE_TEST_SUITE_P(
    NoDelay,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(0, 0, 0, 0, 0.0, 0)));

INSTANTIATE_TEST_SUITE_P(
    HighLatency,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(0, 0, 300, 30, 0.0, 0),
                      NetworkPerformanceParams(0, 0, 30, 10, 0.0, 0)));

INSTANTIATE_TEST_SUITE_P(
    OutOfOrder,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(0, 0, 2, 0, 0.01, 0),
                      NetworkPerformanceParams(0, 0, 30, 1, 0.01, 0),
                      NetworkPerformanceParams(0, 0, 30, 1, 0.1, 0),
                      NetworkPerformanceParams(0, 0, 300, 20, 0.01, 0),
                      NetworkPerformanceParams(0, 0, 300, 20, 0.1, 0)));

INSTANTIATE_TEST_SUITE_P(
    LimitedBandwidth,
    ProtocolPerfTest,
    ::testing::Values(
        // 100 Mbps
        NetworkPerformanceParams(100000, 0.25, 2, 1, 0.0, 0),
        NetworkPerformanceParams(100000, 1.0, 2, 1, 0.0, 0),
        // 8 Mbps
        NetworkPerformanceParams(8000, 0.25, 30, 5, 0.01, 0),
        NetworkPerformanceParams(8000, 1.0, 30, 5, 0.01, 0),
        // 2 Mbps
        NetworkPerformanceParams(2000, 0.25, 30, 5, 0.01, 0),
        NetworkPerformanceParams(2000, 1.0, 30, 5, 0.01, 0),
        // 800 kbps
        NetworkPerformanceParams(800, 0.25, 130, 5, 0.00, 0),
        NetworkPerformanceParams(800, 1.0, 130, 5, 0.00, 0)));

INSTANTIATE_TEST_SUITE_P(
    SlowSignaling,
    ProtocolPerfTest,
    ::testing::Values(NetworkPerformanceParams(8000, 0.25, 30, 0, 0.0, 50),
                      NetworkPerformanceParams(8000, 0.25, 30, 0, 0.0, 500)));

// TotalLatency[Ice|Webrtc] tests measure video latency in the case when the
// whole screen is updated occasionally. It's intended to simulate the case when
// user actions (e.g. Alt-Tab, click on the task bar) cause whole screen to be
// updated.
void ProtocolPerfTest::MeasureTotalLatency(bool use_webrtc) {
  scoped_refptr<test::CyclicFrameGenerator> frame_generator =
      test::CyclicFrameGenerator::Create();
  desktop_environment_factory_->set_frame_generator(base::BindRepeating(
      &test::CyclicFrameGenerator::GenerateFrame, frame_generator));
  event_timestamp_source_ = frame_generator;

  StartHostAndClient(use_webrtc);
  ASSERT_NO_FATAL_FAILURE(WaitConnected());

  int total_frames = 0;

  const base::TimeDelta kWarmUpTime = base::Seconds(2);
  const base::TimeDelta kTestTime = base::Seconds(5);

  base::TimeTicks start_time = base::TimeTicks::Now();
  while ((base::TimeTicks::Now() - start_time) < (kWarmUpTime + kTestTime)) {
    ReceiveFrame();
    ++total_frames;
  }

  WaitFrameStats(total_frames);

  int warm_up_frames = 0;

  int big_update_count = 0;
  base::TimeDelta total_latency_big_updates;
  int small_update_count = 0;
  base::TimeDelta total_latency_small_updates;
  for (int i = 0; i < total_frames; ++i) {
    const protocol::FrameStats& stats = frame_stats_[i];

    // CyclicFrameGenerator::TakeLastEventTimestamps() always returns non-null
    // timestamps.
    CHECK(!stats.host_stats.latest_event_timestamp.is_null());

    test::CyclicFrameGenerator::ChangeInfoList changes =
        frame_generator->GetChangeList(stats.host_stats.latest_event_timestamp);

    // Allow 2 seconds for the connection to warm-up, e.g. to get bandwidth
    // estimate, etc. These frames are ignored when calculating stats below.
    if (stats.client_stats.time_rendered < (start_time + kWarmUpTime)) {
      ++warm_up_frames;
      continue;
    }

    for (auto& change_info : changes) {
      base::TimeDelta latency =
          stats.client_stats.time_rendered - change_info.timestamp;
      switch (change_info.type) {
        case test::CyclicFrameGenerator::ChangeType::NO_CHANGES:
          NOTREACHED();
        case test::CyclicFrameGenerator::ChangeType::FULL:
          total_latency_big_updates += latency;
          ++big_update_count;
          break;
        case test::CyclicFrameGenerator::ChangeType::CURSOR:
          total_latency_small_updates += latency;
          ++small_update_count;
          break;
      }
    }
  }

  WaitFrameStats(total_frames);

  CHECK(big_update_count);
  VLOG(0) << "Average latency for big updates: "
          << (total_latency_big_updates / big_update_count).InMillisecondsF();

  if (small_update_count) {
    VLOG(0)
        << "Average latency for small updates: "
        << (total_latency_small_updates / small_update_count).InMillisecondsF();
  }

  double average_bwe =
      std::accumulate(frame_stats_.begin() + warm_up_frames,
                      frame_stats_.begin() + total_frames, 0.0,
                      [](double sum, const protocol::FrameStats& stats) {
                        return sum + stats.host_stats.bandwidth_estimate_kbps;
                      }) /
      (total_frames - warm_up_frames);
  VLOG(0) << "Average BW estimate: " << average_bwe
          << " (actual: " << GetParam().bandwidth_kbps << ")";
}

TEST_P(ProtocolPerfTest, TotalLatencyIce) {
  MeasureTotalLatency(false);
}

TEST_P(ProtocolPerfTest, TotalLatencyWebrtc) {
  MeasureTotalLatency(true);
}

// ScrollPerformance[Ice|Webrtc] tests simulate whole screen being scrolled
// continuously. They measure FPS and video latency.
void ProtocolPerfTest::MeasureScrollPerformance(bool use_webrtc) {
  scoped_refptr<test::ScrollFrameGenerator> frame_generator =
      new test::ScrollFrameGenerator();
  desktop_environment_factory_->set_frame_generator(base::BindRepeating(
      &test::ScrollFrameGenerator::GenerateFrame, frame_generator));
  event_timestamp_source_ = frame_generator;

  StartHostAndClient(use_webrtc);
  ASSERT_NO_FATAL_FAILURE(WaitConnected());

  const base::TimeDelta kWarmUpTime = base::Seconds(2);
  const base::TimeDelta kTestTime = base::Seconds(2);

  int num_frames = 0;
  int warm_up_frames = 0;
  base::TimeTicks start_time = base::TimeTicks::Now();
  while ((base::TimeTicks::Now() - start_time) < (kTestTime + kWarmUpTime)) {
    ReceiveFrame();
    ++num_frames;

    // Allow 2 seconds for the connection to warm-up, e.g. to get bandwidth
    // estimate, etc. These frames are ignored when calculating stats below.
    if ((base::TimeTicks::Now() - start_time) < kWarmUpTime) {
      ++warm_up_frames;
      client_socket_factory_->ResetStats();
    }
  }

  base::TimeDelta total_time = (base::TimeTicks::Now() - start_time);

  WaitFrameStats(warm_up_frames + num_frames);

  int total_size =
      std::accumulate(frame_stats_.begin() + warm_up_frames,
                      frame_stats_.begin() + warm_up_frames + num_frames, 0,
                      [](int sum, const protocol::FrameStats& stats) {
                        return sum + stats.host_stats.frame_size;
                      });

  base::TimeDelta latency_sum = std::accumulate(
      frame_stats_.begin() + warm_up_frames,
      frame_stats_.begin() + warm_up_frames + num_frames, base::TimeDelta(),
      [](base::TimeDelta sum, const protocol::FrameStats& stats) {
        return sum + (stats.client_stats.time_rendered -
                      stats.host_stats.latest_event_timestamp);
      });

  double average_bwe =
      std::accumulate(frame_stats_.begin() + warm_up_frames,
                      frame_stats_.begin() + warm_up_frames + num_frames, 0.0,
                      [](double sum, const protocol::FrameStats& stats) {
                        return sum + stats.host_stats.bandwidth_estimate_kbps;
                      }) /
      num_frames;

  VLOG(0) << "FPS: " << num_frames / total_time.InSecondsF();
  VLOG(0) << "Average latency: " << latency_sum.InMillisecondsF() / num_frames
          << " ms";
  VLOG(0) << "Total size: " << total_size << " bytes";
  VLOG(0) << "Bandwidth utilization: "
          << 100 * total_size /
                 (total_time.InSecondsF() * GetParam().bandwidth_kbps * 1000 /
                  8)
          << "%";
  VLOG(0) << "Network buffer delay (bufferbloat), average: "
          << client_socket_factory_->average_buffer_delay().InMilliseconds()
          << " ms,  max:"
          << client_socket_factory_->max_buffer_delay().InMilliseconds()
          << " ms";
  VLOG(0) << "Packet drop rate: " << client_socket_factory_->drop_rate();
  VLOG(0) << "Average BW estimate: " << average_bwe
          << " (actual: " << GetParam().bandwidth_kbps << ")";
}

TEST_P(ProtocolPerfTest, ScrollPerformanceIce) {
  MeasureScrollPerformance(false);
}

TEST_P(ProtocolPerfTest, ScrollPerformanceWebrtc) {
  MeasureScrollPerformance(true);
}

}  // namespace remoting
