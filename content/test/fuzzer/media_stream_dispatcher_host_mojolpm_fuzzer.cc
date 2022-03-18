// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"

#include "content/test/fuzzer/media_stream_dispatcher_host_mojolpm_fuzzer.pb.h"

#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"

#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

const char* kCmdline[] = {"media_stream_dispatcher_host_mojolpm_fuzzer",
                          nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<content::mojolpm::FuzzerEnvironment> environment(
      1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

class MediaStreamDispatcherHostTestcase
    : public content::RenderViewHostTestHarness {
 public:
  explicit MediaStreamDispatcherHostTestcase(
      const content::fuzzing::media_stream_dispatcher_host::proto::Testcase&
          testcase);
  ~MediaStreamDispatcherHostTestcase() override;

  bool IsFinished();
  void NextAction();

  // Prerequisite state.
  base::SimpleTestTickClock clock_;

 private:
  using Action = content::fuzzing::media_stream_dispatcher_host::proto::Action;

  void SetUp() override;
  void SetUpOnUIThread();

  void TearDown() override;
  void TearDownOnUIThread();

  void AddMediaStreamDispatcherHost(uint32_t id);
  void AddMediaStreamDispatcherHostImpl();

  void TestBody() override {}

  // The proto message describing the test actions to perform.
  const content::fuzzing::media_stream_dispatcher_host::proto::Testcase&
      testcase_;

  // Apply a reasonable upper-bound on testcase complexity to avoid timeouts.
  const int max_action_count_ = 512;

  // Apply a reasonable upper-bound on maximum size of action that we will
  // deserialize. (This is deliberately slightly larger than max mojo message
  // size)
  const size_t max_action_size_ = 300 * 1024 * 1024;

  // Count of total actions performed in this testcase.
  int action_count_ = 0;

  // The index of the next sequence of actions to execute.
  int next_sequence_idx_ = 0;

  std::unique_ptr<media::AudioSystem> audio_system_ = nullptr;
  std::unique_ptr<content::MediaStreamManager> media_stream_manager_ = nullptr;
  content::MediaStreamDispatcherHost* media_stream_dispatcher_host_ = nullptr;
  content::TestRenderFrameHost* render_frame_host_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

MediaStreamDispatcherHostTestcase::MediaStreamDispatcherHostTestcase(
    const content::fuzzing::media_stream_dispatcher_host::proto::Testcase&
        testcase)
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          content::BrowserTaskEnvironment::REAL_IO_THREAD),
      testcase_(testcase) {
  SetUp();
}

MediaStreamDispatcherHostTestcase::~MediaStreamDispatcherHostTestcase() {
  TearDown();
}

bool MediaStreamDispatcherHostTestcase::IsFinished() {
  return next_sequence_idx_ >= testcase_.sequence_indexes_size();
}

void MediaStreamDispatcherHostTestcase::NextAction() {
  if (next_sequence_idx_ < testcase_.sequence_indexes_size()) {
    auto sequence_idx = testcase_.sequence_indexes(next_sequence_idx_++);
    const auto& sequence =
        testcase_.sequences(sequence_idx % testcase_.sequences_size());
    for (auto action_idx : sequence.action_indexes()) {
      if (!testcase_.actions_size() || ++action_count_ > max_action_count_) {
        return;
      }
      const auto& action =
          testcase_.actions(action_idx % testcase_.actions_size());
      if (action.ByteSizeLong() > max_action_size_) {
        return;
      }
      switch (action.action_case()) {
        case Action::kRunThread: {
          if (action.run_thread().id()) {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            content::GetUIThreadTaskRunner({})->PostTask(
                FROM_HERE, run_loop.QuitClosure());
            run_loop.Run();
          } else {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            content::GetIOThreadTaskRunner({})->PostTask(
                FROM_HERE, run_loop.QuitClosure());
            run_loop.Run();
          }
        } break;
        case Action::kNewMediaStreamDispatcherHost: {
          AddMediaStreamDispatcherHost(
              action.new_media_stream_dispatcher_host().id());
        } break;
        case Action::kMediaStreamDispatcherHostRemoteAction: {
          mojolpm::HandleRemoteAction(
              action.media_stream_dispatcher_host_remote_action());
        } break;
        case Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void MediaStreamDispatcherHostTestcase::SetUp() {
  RenderViewHostTestHarness::SetUp();
  base::RunLoop run_loop;

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&MediaStreamDispatcherHostTestcase::SetUpOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void MediaStreamDispatcherHostTestcase::SetUpOnUIThread() {
  // content::CreateAudioSystemForAudioService();
  render_frame_host_ =
      static_cast<content::TestWebContents*>(web_contents())->GetMainFrame();
  render_frame_host_->InitializeRenderFrameIfNeeded();

  // UI thread
  audio_system_ = content::CreateAudioSystemForAudioService();

  // UI thread
  media_stream_manager_ = std::make_unique<content::MediaStreamManager>(
      audio_system_.get(), content::GetIOThreadTaskRunner({}));
}

void MediaStreamDispatcherHostTestcase::TearDown() {
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&MediaStreamDispatcherHostTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();

  RenderViewHostTestHarness::TearDown();
}

void MediaStreamDispatcherHostTestcase::TearDownOnUIThread() {
  // media_stream_manager_.reset();
  // audio_system_.reset();
}

void MediaStreamDispatcherHostTestcase::AddMediaStreamDispatcherHostImpl() {
  // BrowserIO thread
  media_stream_dispatcher_host_ = new content::MediaStreamDispatcherHost(
      render_frame_host_->GetProcess()->GetID(),
      render_frame_host_->GetRoutingID(), media_stream_manager_.get());
}

// Component(s) which we fuzz
void MediaStreamDispatcherHostTestcase::AddMediaStreamDispatcherHost(
    uint32_t id) {
  mojo::Remote<blink::mojom::MediaStreamDispatcherHost> remote;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  auto receiver = remote.BindNewPipeAndPassReceiver();

  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamDispatcherHostTestcase::AddMediaStreamDispatcherHostImpl,
          base::Unretained(this)),
      run_loop.QuitClosure());

  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}

// Helper function to keep scheduling fuzzer actions on the current runloop
// until the testcase has completed, and then quit the runloop.
void NextAction(MediaStreamDispatcherHostTestcase* testcase,
                base::RepeatingClosure quit_closure) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(quit_closure)));
  } else {
    GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(quit_closure));
  }
}

// Helper function to setup and run the testcase, since we need to do that from
// the fuzzer sequence rather than the main thread.
void RunTestcase(MediaStreamDispatcherHostTestcase* testcase) {
  mojo::Message message;
  auto dispatch_context =
      std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  mojolpm::GetContext()->StartTestcase();

  base::RunLoop fuzzer_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  fuzzer_run_loop.Run();

  mojolpm::GetContext()->EndTestcase();
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::media_stream_dispatcher_host::proto::Testcase&
        proto_testcase) {
  // bail out early if no work to do
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  // Make sure that the environment is initialized before we do anything else.
  GetEnvironment();

  MediaStreamDispatcherHostTestcase testcase(proto_testcase);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Unretained is safe here, because ui_run_loop has to finish before
  // testcase goes out of scope.
  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
