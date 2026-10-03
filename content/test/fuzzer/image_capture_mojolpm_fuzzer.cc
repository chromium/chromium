// Copyright 2022 The Chromium Authors
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
#include "content/browser/image_capture/image_capture_impl.h"  //nogncheck
#include "content/browser/renderer_host/media/media_stream_manager.h"  //nogncheck
#include "content/browser/site_instance_impl.h"  //nogncheck
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/image_capture_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "media/audio/audio_manager.h"
#include "media/audio/audio_system_impl.h"
#include "media/audio/audio_thread_impl.h"
#include "media/capture/mojom/image_capture.mojom-mojolpm.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

constexpr const char* kCmdline[] = {
    "image_capture_mojolpm_fuzzer",
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

class ImageCaptureTestcase
    : public mojolpm::Testcase<content::fuzzing::image_capture::proto::Testcase,
                               content::fuzzing::image_capture::proto::Action> {
 public:
  using ProtoTestcase = content::fuzzing::image_capture::proto::Testcase;
  using ProtoAction = content::fuzzing::image_capture::proto::Action;

  explicit ImageCaptureTestcase(const ProtoTestcase& testcase);
  ~ImageCaptureTestcase();

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;
  void RunAction(const ProtoAction& action,
                 base::OnceClosure run_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);
  void SetUpOnIOThread(base::OnceClosure done_closure);
  void TearDownOnUIThread(base::OnceClosure done_closure);
  void TearDownOnIOThread(base::OnceClosure done_closure);

  void AddImageCapture(uint32_t id, base::OnceClosure done_closure);
  void AddImageCaptureImpl(
      mojo::PendingReceiver<media::mojom::ImageCapture> receiver);

  // Prerequisite state.
  content::mojolpm::RenderViewHostTestHarnessAdapter test_adapter_;
  std::unique_ptr<media::AudioManager> audio_manager_;
  std::unique_ptr<content::MediaStreamManager> media_stream_manager_;
  std::unique_ptr<media::AudioSystem> audio_system_;
  raw_ptr<content::TestRenderFrameHost> render_frame_host_ = nullptr;
};

ImageCaptureTestcase::ImageCaptureTestcase(const ProtoTestcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase) {
  test_adapter_.SetUp();
}

ImageCaptureTestcase::~ImageCaptureTestcase() {
  test_adapter_.TearDown();
}

void ImageCaptureTestcase::RunAction(const ProtoAction& action,
                                     base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI =
      content::fuzzing::image_capture::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::image_capture::proto::RunThreadAction_ThreadId_IO;

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

    case ProtoAction::kNewImageCapture:
      AddImageCapture(action.new_image_capture().id(), std::move(run_closure));
      return;

    case ProtoAction::kImageCaptureRemoteAction:
      mojolpm::HandleRemoteAction(action.image_capture_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void ImageCaptureTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageCaptureTestcase::SetUpOnIOThread,
                     base::Unretained(this), std::move(done_closure)));
}

void ImageCaptureTestcase::SetUpOnIOThread(base::OnceClosure done_closure) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageCaptureTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void ImageCaptureTestcase::SetUpOnUIThread(base::OnceClosure done_closure) {
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

void ImageCaptureTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageCaptureTestcase::TearDownOnIOThread,
                     base::Unretained(this), std::move(done_closure)));
}

void ImageCaptureTestcase::TearDownOnIOThread(base::OnceClosure done_closure) {
  media_stream_manager_->WillDestroyCurrentMessageLoop();

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ImageCaptureTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void ImageCaptureTestcase::TearDownOnUIThread(base::OnceClosure done_closure) {
  audio_manager_->Shutdown();

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void ImageCaptureTestcase::AddImageCaptureImpl(
    mojo::PendingReceiver<media::mojom::ImageCapture> receiver) {
  content::ImageCaptureImpl::Create(render_frame_host_, std::move(receiver));
}

static void AddImageCaptureInstance(
    uint32_t id,
    mojo::Remote<media::mojom::ImageCapture> remote,
    base::OnceClosure run_closure) {
  mojolpm::GetContext()->AddInstance(id, std::move(remote));
  std::move(run_closure).Run();
}

void ImageCaptureTestcase::AddImageCapture(uint32_t id,
                                           base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<media::mojom::ImageCapture> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ImageCaptureTestcase::AddImageCaptureImpl,
                     base::Unretained(this), std::move(receiver)),
      base::BindOnce(AddImageCaptureInstance, id, std::move(remote),
                     std::move(run_closure)));
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::image_capture::proto::Testcase& proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  GetEnvironment();

  ImageCaptureTestcase testcase(proto_testcase);

  base::RunLoop main_run_loop;

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<ImageCaptureTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));

  main_run_loop.Run();
}
