// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
#include "content/browser/renderer_host/clipboard_host_impl.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/clipboard_host_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/clipboard/clipboard.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"

const char* const kCmdline[] = {"clipboard_host_mojolpm_fuzzer", nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<content::mojolpm::FuzzerEnvironment> environment(
      1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

// Per-testcase state needed to run the interface being tested.
//
// The lifetime of this is scoped to a single testcase, and it is created and
// destroyed from the fuzzer sequence.
//
class ClipboardHostTestcase
    : public ::mojolpm::Testcase<
          content::fuzzing::clipboard_host::proto::Testcase,
          content::fuzzing::clipboard_host::proto::Action> {
 public:
  using ProtoTestcase = content::fuzzing::clipboard_host::proto::Testcase;
  using ProtoAction = content::fuzzing::clipboard_host::proto::Action;
  explicit ClipboardHostTestcase(
      const content::fuzzing::clipboard_host::proto::Testcase& testcase);
  ~ClipboardHostTestcase();

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);
  void TearDownOnUIThread(base::OnceClosure done_closure);

  void AddClipboardHostImpl(
      uint32_t id,
      mojo::PendingReceiver<blink::mojom::ClipboardHost>&& receiver);
  void AddClipboardHost(uint32_t id, base::OnceClosure done_closure);

  content::mojolpm::RenderViewHostTestHarnessAdapter test_adapter_;
  raw_ptr<content::TestRenderFrameHost> render_frame_host_ = nullptr;
};

ClipboardHostTestcase::ClipboardHostTestcase(const ProtoTestcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase) {
  test_adapter_.SetUp();
}

ClipboardHostTestcase::~ClipboardHostTestcase() {
  test_adapter_.TearDown();
}

void ClipboardHostTestcase::RunAction(const ProtoAction& action,
                                      base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);
  const auto ThreadId_UI =
      content::fuzzing::clipboard_host::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::clipboard_host::proto::RunThreadAction_ThreadId_IO;
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
    case ProtoAction::kNewClipboardHost:
      AddClipboardHost(action.new_clipboard_host().id(),
                       std::move(run_closure));
      return;
    case ProtoAction::kClipboardHostRemoteAction:
      mojolpm::HandleRemoteAction(action.clipboard_host_remote_action());
      break;
    case ProtoAction::ACTION_NOT_SET:
      break;
  }
  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
}

void ClipboardHostTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHostTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void ClipboardHostTestcase::SetUpOnUIThread(base::OnceClosure done_closure) {
  render_frame_host_ =
      static_cast<content::TestWebContents*>(test_adapter_.web_contents())
          ->GetPrimaryMainFrame();
  render_frame_host_->InitializeRenderFrameIfNeeded();

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void ClipboardHostTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&ClipboardHostTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void ClipboardHostTestcase::TearDownOnUIThread(base::OnceClosure done_closure) {
  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void ClipboardHostTestcase::AddClipboardHostImpl(
    uint32_t id,
    mojo::PendingReceiver<blink::mojom::ClipboardHost>&& receiver) {
  content::ClipboardHostImpl::Create(render_frame_host_, std::move(receiver));
}

static void AddClipboardHostInstance(
    uint32_t id,
    mojo::Remote<blink::mojom::ClipboardHost> remote,
    base::OnceClosure run_closure) {
  mojolpm::GetContext()->AddInstance(id, std::move(remote));
  std::move(run_closure).Run();
}

void ClipboardHostTestcase::AddClipboardHost(uint32_t id,
                                             base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(this->sequence_checker_);
  mojo::Remote<blink::mojom::ClipboardHost> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ClipboardHostTestcase::AddClipboardHostImpl,
                     base::Unretained(this), id, std::move(receiver)),
      base::BindOnce(AddClipboardHostInstance, id, std::move(remote),
                     std::move(run_closure)));
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::clipboard_host::proto::Testcase& proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  GetEnvironment();

  ClipboardHostTestcase testcase(proto_testcase);

  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<ClipboardHostTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}
