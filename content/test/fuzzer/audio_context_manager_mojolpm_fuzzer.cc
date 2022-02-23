// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <cstddef>
#include <utility>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/i18n/icu_util.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"  // [nogncheck]
#include "content/browser/network_service_instance_impl.h"  // nogncheck
#include "content/browser/site_instance_impl.h"             // nogncheck
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fuzzer/audio_context_manager_mojolpm_fuzzer.pb.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

const char* const cmdline[] = {"audio_context_manager_mojolpm_fuzzer", nullptr};
class ContentFuzzerEnvironment {
 public:
  ContentFuzzerEnvironment()
      : fuzzer_thread_((base::CommandLine::Init(1, cmdline), "fuzzer_thread")) {
    TestTimeouts::Initialize();
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    base::i18n::InitializeICU();
    fuzzer_thread_.StartAndWaitForTesting();

    content::ForceCreateNetworkServiceDirectlyForTesting();
  }

  scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner() {
    return fuzzer_thread_.task_runner();
  }

 private:
  base::AtExitManager at_exit_manager_;
  base::Thread fuzzer_thread_;
  content::TestContentClientInitializer content_client_initializer_;
};

ContentFuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<ContentFuzzerEnvironment> environment;
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

class AudioContextManagerTestcase : public content::RenderViewHostTestHarness {
 public:
  explicit AudioContextManagerTestcase(
      const content::fuzzing::audio_context_manager::proto::Testcase& testcase);
  ~AudioContextManagerTestcase() override;

  bool IsFinished();
  void NextAction();

  // Prerequisite state.
  base::SimpleTestTickClock clock_;

 private:
  using Action = content::fuzzing::audio_context_manager::proto::Action;

  void SetUp() override;
  void SetUpOnUIThread();

  void TearDown() override;
  void TearDownOnUIThread();

  void AddAudioContextManager(uint32_t id);
  void AddAudioContextManagerImpl(
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);

  void TestBody() override {}

  // The proto message describing the test actions to perform.
  const content::fuzzing::audio_context_manager::proto::Testcase& testcase_;

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

  content::AudioContextManagerImpl* audio_context_manager_ = nullptr;
  content::TestRenderFrameHost* render_frame_host_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

AudioContextManagerTestcase::AudioContextManagerTestcase(
    const content::fuzzing::audio_context_manager::proto::Testcase& testcase)
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          content::BrowserTaskEnvironment::REAL_IO_THREAD),
      testcase_(testcase) {
  SetUp();
}

AudioContextManagerTestcase::~AudioContextManagerTestcase() {
  TearDown();
}

bool AudioContextManagerTestcase::IsFinished() {
  return next_sequence_idx_ >= testcase_.sequence_indexes_size();
}

void AudioContextManagerTestcase::NextAction() {
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
        case Action::kNewAudioContextManager: {
          AddAudioContextManager(action.new_audio_context_manager().id());
        } break;
        // TODO(bookholt): add support for playback start/stop
        case Action::kAudioContextManagerRemoteAction: {
          mojolpm::HandleRemoteAction(
              action.audio_context_manager_remote_action());
        } break;
        case Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void AudioContextManagerTestcase::SetUp() {
  RenderViewHostTestHarness::SetUp();
  base::RunLoop run_loop;

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AudioContextManagerTestcase::SetUpOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void AudioContextManagerTestcase::SetUpOnUIThread() {
  // content::CreateAudioSystemForAudioService();
  render_frame_host_ =
      static_cast<content::TestWebContents*>(web_contents())->GetMainFrame();
  render_frame_host_->InitializeRenderFrameIfNeeded();
}

void AudioContextManagerTestcase::TearDown() {
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AudioContextManagerTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();

  RenderViewHostTestHarness::TearDown();
}

void AudioContextManagerTestcase::TearDownOnUIThread() {
  // audio_context_manager_.reset();
}

void AudioContextManagerTestcase::AddAudioContextManagerImpl(
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  clock_.SetNowTicks(base::TimeTicks::Now());

  audio_context_manager_ = new content::AudioContextManagerImpl(
      render_frame_host_, std::move(receiver));

  audio_context_manager_->set_clock_for_testing(&clock_);
}

// Component(s) which we fuzz
void AudioContextManagerTestcase::AddAudioContextManager(uint32_t id) {
  mojo::Remote<blink::mojom::AudioContextManager> remote;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  auto receiver = remote.BindNewPipeAndPassReceiver();

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AudioContextManagerTestcase::AddAudioContextManagerImpl,
                     base::Unretained(this), std::move(receiver)),
      run_loop.QuitClosure());
  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}

// Helper function to keep scheduling fuzzer actions on the current runloop
// until the testcase has completed, and then quit the runloop.
void NextAction(AudioContextManagerTestcase* testcase,
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
void RunTestcase(AudioContextManagerTestcase* testcase) {
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
    const content::fuzzing::audio_context_manager::proto::Testcase&
        proto_testcase) {
  // bail out early if no work to do
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  // Make sure that the environment is initialized before we do anything else.
  GetEnvironment();

  AudioContextManagerTestcase testcase(proto_testcase);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Unretained is safe here, because ui_run_loop has to finish before
  // testcase goes out of scope.
  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
