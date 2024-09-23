// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/media/media_internals.h"
#include "content/browser/renderer_host/media/media_stream_dispatcher_host.h"
#include "content/browser/renderer_host/media/media_stream_manager.h"
#include "content/public/browser/audio_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/media_stream_dispatcher_host_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/mediastream/media_stream.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

const char* kCmdline[] = {
    "media_stream_dispatcher_host_mojolpm_fuzzer",
    "--use-fake-device-for-media-stream",  // Make sure we use fake devices to
                                           // avoid long delays.
    nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<content::mojolpm::FuzzerEnvironment> environment(
      2, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

class MediaStreamDispatcherHostTestcase
    : public mojolpm::Testcase<
          content::fuzzing::media_stream_dispatcher_host::proto::Testcase,
          content::fuzzing::media_stream_dispatcher_host::proto::Action> {
 public:
  using ProtoTestcase =
      content::fuzzing::media_stream_dispatcher_host::proto::Testcase;
  using ProtoAction =
      content::fuzzing::media_stream_dispatcher_host::proto::Action;

  explicit MediaStreamDispatcherHostTestcase(const ProtoTestcase& testcase);
  ~MediaStreamDispatcherHostTestcase();

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);

  void TearDownOnIOThread();
  void TearDownOnUIThread(base::OnceClosure done_closure);

  void AddMediaStreamDispatcherHost(uint32_t id,
                                    base::OnceClosure done_closure);
  void AddMediaStreamDispatcherHostImpl(
      mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost>&&
          receiver);

  // Prerequisite state.
  content::mojolpm::RenderViewHostTestHarnessAdapter test_adapter_;
  std::unique_ptr<media::AudioManager> audio_manager_ = nullptr;
  std::unique_ptr<media::AudioSystem> audio_system_ = nullptr;
  std::unique_ptr<content::MediaStreamManager> media_stream_manager_ = nullptr;
  raw_ptr<content::TestRenderFrameHost> render_frame_host_ = nullptr;
};

MediaStreamDispatcherHostTestcase::MediaStreamDispatcherHostTestcase(
    const ProtoTestcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase) {
  test_adapter_.SetUp();
}

MediaStreamDispatcherHostTestcase::~MediaStreamDispatcherHostTestcase() {
  test_adapter_.TearDown();
  audio_manager_.reset();
  media_stream_manager_.reset();
}

void MediaStreamDispatcherHostTestcase::RunAction(
    const ProtoAction& action,
    base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI = content::fuzzing::media_stream_dispatcher_host::
      proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO = content::fuzzing::media_stream_dispatcher_host::
      proto::RunThreadAction_ThreadId_IO;

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

    case ProtoAction::kNewMediaStreamDispatcherHost:
      AddMediaStreamDispatcherHost(
          action.new_media_stream_dispatcher_host().id(),
          std::move(run_closure));
      return;

    case ProtoAction::kMediaStreamDispatcherHostRemoteAction:
      mojolpm::HandleRemoteAction(
          action.media_stream_dispatcher_host_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void MediaStreamDispatcherHostTestcase::SetUp(base::OnceClosure done_closure) {
  mojolpm::GetContext()->StartTestcase();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamDispatcherHostTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void MediaStreamDispatcherHostTestcase::SetUpOnUIThread(
    base::OnceClosure done_closure) {
  render_frame_host_ =
      static_cast<content::TestWebContents*>(test_adapter_.web_contents())
          ->GetPrimaryMainFrame();

  render_frame_host_->InitializeRenderFrameIfNeeded();

  audio_manager_ = media::AudioManager::CreateForTesting(
      std::make_unique<media::AudioThreadImpl>());

  audio_system_ =
      std::make_unique<media::AudioSystemImpl>(audio_manager_.get());

  media_stream_manager_ =
      std::make_unique<content::MediaStreamManager>(audio_system_.get());

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void MediaStreamDispatcherHostTestcase::TearDown(
    base::OnceClosure done_closure) {
  mojolpm::GetContext()->EndTestcase();

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamDispatcherHostTestcase::TearDownOnIOThread,
                     base::Unretained(this)));

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&MediaStreamDispatcherHostTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void MediaStreamDispatcherHostTestcase::TearDownOnIOThread() {
  media_stream_manager_->WillDestroyCurrentMessageLoop();
}

void MediaStreamDispatcherHostTestcase::TearDownOnUIThread(
    base::OnceClosure done_closure) {
  audio_manager_->Shutdown();

  std::move(done_closure).Run();
}

void MediaStreamDispatcherHostTestcase::AddMediaStreamDispatcherHostImpl(
    mojo::PendingReceiver<blink::mojom::MediaStreamDispatcherHost>&& receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  // MediaStreamDispatcherHost is a self-owned receiver.
  content::MediaStreamDispatcherHost::Create(render_frame_host_->GetGlobalId(),
                                             media_stream_manager_.get(),
                                             std::move(receiver));
}

// Component(s) which we fuzz
static void AddMediaStreamDispatcherHostInstance(
    uint32_t id,
    mojo::Remote<::blink::mojom::MediaStreamDispatcherHost>&& remote,
    base::OnceClosure run_closure) {
  mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(run_closure).Run();
}

void MediaStreamDispatcherHostTestcase::AddMediaStreamDispatcherHost(
    uint32_t id,
    base::OnceClosure done_closure) {
  mojo::Remote<blink::mojom::MediaStreamDispatcherHost> remote;

  auto receiver = remote.BindNewPipeAndPassReceiver();

  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(
          &MediaStreamDispatcherHostTestcase::AddMediaStreamDispatcherHostImpl,
          base::Unretained(this), std::move(receiver)),
      base::BindOnce(AddMediaStreamDispatcherHostInstance, id,
                     std::move(remote), std::move(done_closure)));
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

  base::RunLoop main_run_loop;

  // Unretained is safe here, because `main_run_loop` has to finish before
  // testcase goes out of scope.
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<MediaStreamDispatcherHostTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));

  main_run_loop.Run();
}
