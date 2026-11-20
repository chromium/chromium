// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/perfetto_tracing.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/task_environment.h"
#include "base/test/tracing/trace_test_utils.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"
#include "third_party/perfetto/protos/perfetto/trace/interned_data/interned_data.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/trace_packet.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/process_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/thread_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_descriptor.pbzero.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/track_event.pbzero.h"
#include "third_party/webrtc_overrides/init_webrtc.h"

namespace remoting {

namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;

struct ParsedDescriptorSummary {
  std::set<std::string> process_names;
  std::set<int32_t> process_pids;
  bool any_process_has_cmdline = false;
  bool process_disallows_system_track_merging = false;
  std::set<std::string> thread_names;
  std::set<std::string> event_names;
};

perfetto::TraceConfig BuildTestTraceConfig() {
  perfetto::TraceConfig config;
  config.add_buffers()->set_size_kb(1024);
  auto* ds_cfg = config.add_data_sources()->mutable_config();
  ds_cfg->set_name("track_event");
  perfetto::protos::gen::TrackEventConfig te_cfg;
  te_cfg.add_disabled_categories("*");
  te_cfg.add_enabled_categories("remoting");
  te_cfg.add_enabled_categories("remoting.*");
  te_cfg.add_enabled_categories("webrtc");
  te_cfg.add_enabled_categories("__metadata");
  ds_cfg->set_track_event_config_raw(te_cfg.SerializeAsString());
  return config;
}

ParsedDescriptorSummary ParseDescriptorSummary(
    const std::vector<char>& raw_trace) {
  ParsedDescriptorSummary summary;
  perfetto::protos::pbzero::Trace::Decoder trace(
      reinterpret_cast<const uint8_t*>(raw_trace.data()), raw_trace.size());
  for (auto packet_it = trace.packet(); packet_it; ++packet_it) {
    perfetto::protos::pbzero::TracePacket::Decoder packet(*packet_it);

    if (packet.has_track_descriptor()) {
      perfetto::protos::pbzero::TrackDescriptor::Decoder desc(
          packet.track_descriptor());
      if (desc.has_process()) {
        summary.process_disallows_system_track_merging =
            desc.disallow_merging_with_system_tracks();
        perfetto::protos::pbzero::ProcessDescriptor::Decoder proc(
            desc.process());
        if (proc.has_process_name()) {
          summary.process_names.insert(proc.process_name().ToStdString());
        }
        if (proc.has_pid()) {
          summary.process_pids.insert(proc.pid());
        }
        if (proc.has_cmdline()) {
          summary.any_process_has_cmdline = true;
        }
      }
      if (desc.has_thread()) {
        perfetto::protos::pbzero::ThreadDescriptor::Decoder thread(
            desc.thread());
        if (thread.has_thread_name()) {
          summary.thread_names.insert(thread.thread_name().ToStdString());
        }
      }
    }

    if (packet.has_interned_data()) {
      perfetto::protos::pbzero::InternedData::Decoder interned(
          packet.interned_data());
      for (auto name_it = interned.event_names(); name_it; ++name_it) {
        perfetto::protos::pbzero::EventName::Decoder event_name(*name_it);
        if (event_name.has_name()) {
          summary.event_names.insert(event_name.name().ToStdString());
        }
      }
    }

    if (packet.has_track_event()) {
      perfetto::protos::pbzero::TrackEvent::Decoder event(packet.track_event());
      if (event.has_name()) {
        summary.event_names.insert(event.name().ToStdString());
      }
    }
  }
  return summary;
}

std::unique_ptr<perfetto::TracingSession> StartTestTraceSession() {
  auto session = perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
  session->Setup(BuildTestTraceConfig());
  session->StartBlocking();
  return session;
}

ParsedDescriptorSummary StopAndParseTestTraceSession(
    std::unique_ptr<perfetto::TracingSession> session) {
  base::TrackEvent::Flush();
  session->FlushBlocking(5000);
  session->StopBlocking();
  std::vector<char> raw_trace = session->ReadTraceBlocking();
  EXPECT_THAT(raw_trace, Not(IsEmpty()));
  return ParseDescriptorSummary(raw_trace);
}

class PerfettoTracingTest : public testing::Test {
 protected:
  void SetUp() override;
  void TearDown() override;

  base::test::TaskEnvironment task_environment_;
};

void PerfettoTracingTest::SetUp() {
  InitPerfettoTracing("CRD UnitTest Process");
}

void PerfettoTracingTest::TearDown() {
  ResetPerfettoTracingForTesting();
}

}  // namespace

TEST_F(PerfettoTracingTest,
       ProcessTrackDescriptorSetsNameAndPidWithoutCmdline) {
  auto session = StartTestTraceSession();
  TRACE_EVENT("remoting", "MainThreadEvent");
  ParsedDescriptorSummary summary =
      StopAndParseTestTraceSession(std::move(session));

  EXPECT_THAT(summary.process_names, Contains("CRD UnitTest Process"));
  EXPECT_THAT(summary.process_pids,
              Contains(static_cast<int32_t>(base::GetCurrentProcId())));
  EXPECT_FALSE(summary.any_process_has_cmdline);
  EXPECT_TRUE(summary.process_disallows_system_track_merging);
}

TEST_F(PerfettoTracingTest, RegistersChromiumAndWebRtcCategories) {
  auto session = StartTestTraceSession();

  const auto& webrtc_registry = GetWebRtcTrackEventCategoryRegistry();
  ASSERT_GT(webrtc_registry.category_count(), 0u);
  EXPECT_NE(
      webrtc_registry.GetCategoryState(0)->load(std::memory_order_relaxed), 0u);

  TRACE_EVENT("remoting", "LifecycleEvent");
  TRACE_EVENT("remoting.video", "VideoEvent");

  ParsedDescriptorSummary summary =
      StopAndParseTestTraceSession(std::move(session));
  EXPECT_THAT(summary.event_names, Contains("LifecycleEvent"));
  EXPECT_THAT(summary.event_names, Contains("VideoEvent"));
}

TEST_F(PerfettoTracingTest, ThreadNameObserverRecordsNamedThreads) {
  base::Thread worker_thread("CRD Capture Thread");
  ASSERT_TRUE(worker_thread.Start());

  auto session = StartTestTraceSession();

  base::RunLoop worker_event_loop;
  worker_thread.task_runner()->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce([]() { TRACE_EVENT("remoting.video", "CaptureEvent"); }),
      worker_event_loop.QuitClosure());
  worker_event_loop.Run();

  ParsedDescriptorSummary summary =
      StopAndParseTestTraceSession(std::move(session));
  EXPECT_THAT(summary.thread_names, Contains("CRD Capture Thread"));
  EXPECT_THAT(summary.event_names, Contains("CaptureEvent"));
}

TEST_F(PerfettoTracingTest, SetCurrentThreadTrackNamePersistsAcrossSessions) {
  auto encoder_runner = base::ThreadPool::CreateSingleThreadTaskRunner(
      {base::TaskPriority::USER_BLOCKING},
      base::SingleThreadTaskRunnerThreadMode::DEDICATED);

  base::RunLoop setup_encoder_name_loop;
  encoder_runner->PostTaskAndReply(
      FROM_HERE, base::BindOnce([]() {
        SetCurrentThreadTrackName("CRD VideoEncoder (screen_0)");
      }),
      setup_encoder_name_loop.QuitClosure());
  setup_encoder_name_loop.Run();

  // Run two back-to-back sessions (including a repeat `InitPerfettoTracing()`
  // call) to verify custom track names are not overwritten by
  // `ThreadIdNameManager`.
  for (int attempt = 0; attempt < 2; ++attempt) {
    InitPerfettoTracing("CRD UnitTest Process");

    auto session = StartTestTraceSession();
    base::RunLoop encoder_event_loop;
    encoder_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce([]() { TRACE_EVENT("remoting.video", "EncoderEvent"); }),
        encoder_event_loop.QuitClosure());
    encoder_event_loop.Run();

    ParsedDescriptorSummary summary =
        StopAndParseTestTraceSession(std::move(session));
    EXPECT_THAT(summary.thread_names, Contains("CRD VideoEncoder (screen_0)"));
    EXPECT_THAT(summary.event_names, Contains("EncoderEvent"));
  }
}

TEST(PerfettoTracingInteroperabilityTest,
     CoexistsWithBaseTracingEnvironmentInSameShard) {
  base::test::TaskEnvironment task_environment;

  // Verify `base::test::TracingEnvironment` initialized before
  // `InitPerfettoTracing`.
  {
    base::test::TracingEnvironment tracing_env;
    InitPerfettoTracing("CRD Coexist Process");

    auto session = StartTestTraceSession();
    TRACE_EVENT("remoting.video", "CoexistEvent1");
    ParsedDescriptorSummary summary =
        StopAndParseTestTraceSession(std::move(session));
    EXPECT_THAT(summary.event_names, Contains("CoexistEvent1"));

    ResetPerfettoTracingForTesting();
  }

  // Verify `base::test::TracingEnvironment` initialized after
  // `InitPerfettoTracing` and `ResetPerfettoTracingForTesting`.
  {
    InitPerfettoTracing("CRD Standalone Process");
    ResetPerfettoTracingForTesting();
    base::test::TracingEnvironment tracing_env;
  }
}

TEST(PerfettoFrameTraceIdTest, GeneratesNonZeroProcessPrefixedIds) {
  const uint64_t id1 = NextPerfettoFrameTraceId();
  const uint64_t id2 = NextPerfettoFrameTraceId();
  EXPECT_NE(id1, 0u);
  EXPECT_NE(id1 & 0xFFFFFFFF00000000ULL, 0u);
  EXPECT_EQ(id1 & 0xFFFFFFFF00000000ULL, id2 & 0xFFFFFFFF00000000ULL);
  EXPECT_EQ((id2 & 0xFFFFFFFFULL), (id1 & 0xFFFFFFFFULL) + 1);
}

}  // namespace remoting
