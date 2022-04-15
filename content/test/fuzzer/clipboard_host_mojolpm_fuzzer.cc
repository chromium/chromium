// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/no_destructor.h"
#include "content/browser/renderer_host/clipboard_host_impl.h"
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
class ClipboardHostTestcase : public content::RenderViewHostTestHarness {
 public:
  explicit ClipboardHostTestcase(
      const content::fuzzing::clipboard_host::proto::Testcase& testcase);
  ~ClipboardHostTestcase() override;

  bool IsFinished();
  void NextAction();

 private:
  using Action = content::fuzzing::clipboard_host::proto::Action;

  void SetUp() override;
  void SetUpOnUIThread();

  void TearDown() override;
  void TearDownOnUIThread();

  void AddClipboardHostImpl(
      mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver);
  void AddClipboardHost(uint32_t id);

  void TestBody() override {}

  // The proto message describing the test actions to perform.
  const content::fuzzing::clipboard_host::proto::Testcase& testcase_;

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

  content::TestRenderFrameHost* render_frame_host_ = nullptr;

  SEQUENCE_CHECKER(sequence_checker_);
};

ClipboardHostTestcase::ClipboardHostTestcase(
    const content::fuzzing::clipboard_host::proto::Testcase& testcase)
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          content::BrowserTaskEnvironment::REAL_IO_THREAD),
      testcase_(testcase) {
  SetUp();
}

ClipboardHostTestcase::~ClipboardHostTestcase() {
  TearDown();
}

bool ClipboardHostTestcase::IsFinished() {
  return next_sequence_idx_ >= testcase_.sequence_indexes_size();
}

void ClipboardHostTestcase::NextAction() {
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
        case Action::kNewClipboardHost: {
          AddClipboardHost(action.new_clipboard_host().id());
        } break;
        case Action::kClipboardHostRemoteAction: {
          mojolpm::HandleRemoteAction(action.clipboard_host_remote_action());
        } break;
        case Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void ClipboardHostTestcase::SetUp() {
  RenderViewHostTestHarness::SetUp();
  base::RunLoop run_loop;

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ClipboardHostTestcase::SetUpOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void ClipboardHostTestcase::SetUpOnUIThread() {
  render_frame_host_ =
      static_cast<content::TestWebContents*>(web_contents())->GetMainFrame();
  render_frame_host_->InitializeRenderFrameIfNeeded();
}

void ClipboardHostTestcase::TearDown() {
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ClipboardHostTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();

  RenderViewHostTestHarness::TearDown();
}

void ClipboardHostTestcase::TearDownOnUIThread() {}

void ClipboardHostTestcase::AddClipboardHostImpl(
    mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver) {
  content::ClipboardHostImpl::Create(render_frame_host_, std::move(receiver));
}

void ClipboardHostTestcase::AddClipboardHost(uint32_t id) {
  mojo::Remote<blink::mojom::ClipboardHost> remote;
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  mojo::PendingReceiver<blink::mojom::ClipboardHost> receiver =
      remote.BindNewPipeAndPassReceiver();

  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&ClipboardHostTestcase::AddClipboardHostImpl,
                     base::Unretained(this), std::move(receiver)),
      run_loop.QuitClosure());

  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}

void NextAction(ClipboardHostTestcase* testcase,
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

void RunTestcase(ClipboardHostTestcase* testcase) {
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
    const content::fuzzing::clipboard_host::proto::Testcase& proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  GetEnvironment();

  ClipboardHostTestcase testcase(proto_testcase);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
