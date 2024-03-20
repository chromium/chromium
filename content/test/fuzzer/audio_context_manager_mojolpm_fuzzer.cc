// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstddef>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/media/webaudio/audio_context_manager_impl.h"  // [nogncheck]
#include "content/browser/site_instance_impl.h"  // nogncheck
#include "content/public/browser/audio_service.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/audio_context_manager_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/webaudio/audio_context_manager.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

const char* kCmdline[] = {"audio_context_manager_mojolpm_fuzzer", nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<content::mojolpm::FuzzerEnvironment> environment(
      1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

class AudioContextManagerTestcase
    : public mojolpm::Testcase<
          content::fuzzing::audio_context_manager::proto::Testcase,
          content::fuzzing::audio_context_manager::proto::Action> {
 public:
  using ProtoTestcase =
      content::fuzzing::audio_context_manager::proto::Testcase;
  using ProtoAction = content::fuzzing::audio_context_manager::proto::Action;

  explicit AudioContextManagerTestcase(const ProtoTestcase& testcase);
  ~AudioContextManagerTestcase();

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;
  void RunAction(const ProtoAction& action,
                 base::OnceClosure run_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);

  void TearDownOnUIThread(base::OnceClosure done_closure);

  void AddAudioContextManager(uint32_t id, base::OnceClosure done_closure);
  void AddAudioContextManagerImpl(
      mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver);

  // Prerequisite state.
  content::mojolpm::RenderViewHostTestHarnessAdapter test_adapter_;

  base::SimpleTestTickClock clock_;
  raw_ptr<content::AudioContextManagerImpl> audio_context_manager_ = nullptr;
  raw_ptr<content::TestRenderFrameHost> render_frame_host_ = nullptr;
};

AudioContextManagerTestcase::AudioContextManagerTestcase(
    const ProtoTestcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase) {
  test_adapter_.SetUp();
}

AudioContextManagerTestcase::~AudioContextManagerTestcase() {
  test_adapter_.TearDown();
}

void AudioContextManagerTestcase::RunAction(const ProtoAction& action,
                                            base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI = content::fuzzing::audio_context_manager::proto::
      RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO = content::fuzzing::audio_context_manager::proto::
      RunThreadAction_ThreadId_IO;

  switch (action.action_case()) {
    case ProtoAction::kRunThread:
      // These actions ensure that any tasks currently queued on the named
      // thread have chance to run before the fuzzer continues.
      //
      // We don't provide any particular guarantees here; this does not mean
      // that the named thread is idle, nor does it prevent any other threads
      // from running (or the consequences of any resulting callbacks, for
      // example).
      if (action.run_thread().id() == ThreadId_UI) {
        content::GetUIThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      } else if (action.run_thread().id() == ThreadId_IO) {
        content::GetIOThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      }
      return;

    case ProtoAction::kNewAudioContextManager:
      AddAudioContextManager(action.new_audio_context_manager().id(),
                             std::move(run_closure));
      return;

    // TODO(bookholt): add support for playback start/stop
    case ProtoAction::kAudioContextManagerRemoteAction:
      mojolpm::HandleRemoteAction(action.audio_context_manager_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void AudioContextManagerTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioContextManagerTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void AudioContextManagerTestcase::SetUpOnUIThread(
    base::OnceClosure done_closure) {
  // content::CreateAudioSystemForAudioService();

  render_frame_host_ =
      static_cast<content::TestWebContents*>(test_adapter_.web_contents())
          ->GetPrimaryMainFrame();
  render_frame_host_->InitializeRenderFrameIfNeeded();

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void AudioContextManagerTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&AudioContextManagerTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void AudioContextManagerTestcase::TearDownOnUIThread(
    base::OnceClosure done_closure) {
  // audio_context_manager_.reset();

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void AudioContextManagerTestcase::AddAudioContextManagerImpl(
    mojo::PendingReceiver<blink::mojom::AudioContextManager> receiver) {
  clock_.SetNowTicks(base::TimeTicks::Now());

  audio_context_manager_ = &content::AudioContextManagerImpl::CreateForTesting(
      *render_frame_host_, std::move(receiver));

  audio_context_manager_->set_clock_for_testing(&clock_);
}

static void AddAudioContextManagerInstance(
    uint32_t id,
    mojo::Remote<blink::mojom::AudioContextManager> remote,
    base::OnceClosure run_closure) {
  mojolpm::GetContext()->AddInstance(id, std::move(remote));
  std::move(run_closure).Run();
}

// Component(s) which we fuzz
void AudioContextManagerTestcase::AddAudioContextManager(
    uint32_t id,
    base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<blink::mojom::AudioContextManager> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&AudioContextManagerTestcase::AddAudioContextManagerImpl,
                     base::Unretained(this), std::move(receiver)),
      base::BindOnce(AddAudioContextManagerInstance, id, std::move(remote),
                     std::move(run_closure)));
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

  base::RunLoop main_run_loop;

  // Unretained is safe here, because `main_run_loop` has to finish before
  // testcase goes out of scope.
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<AudioContextManagerTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));

  main_run_loop.Run();
}
