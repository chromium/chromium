// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ref.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread.h"
#include "content/browser/gpu/gpu_data_manager_impl.h"  // nogncheck
#include "content/browser/presentation/presentation_service_impl.h"  // nogncheck
#include "content/browser/presentation/presentation_test_utils.h"
#include "content/browser/site_instance_impl.h"  // nogncheck
#include "content/public/browser/presentation_request.h"
#include "content/public/browser/presentation_service_delegate.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/fuzzer/controller_presentation_service_delegate_for_fuzzing.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "content/test/fuzzer/presentation_service_mojolpm_fuzzer.pb.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/presentation/presentation.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "ui/events/devices/device_data_manager.h"

const char* kCmdline[] = {"presentation_service_mojolpm_fuzzer", nullptr};

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
// This owns the instance of `PresentationServiceImpl` to be fuzzed, and the
// mock delegate instance that will be used by the service instance.
// This class inherits from `RenderViewHostTestHarness` as the service
// instance relies on using a `RenderFrameHost` instance.
//
// TODO(clovispj) investigate performance loss this causes:
// The test harness has the drawback it owns a `BrowserTaskEnvironment`, so it
// becomes scoped per testcase - it would be preferred to be global to all.
//
// We use a single `PresentationServiceImpl` which can be bound to multiple
// remotes, to imitate true behaviour as much as possbile.
class PresentationServiceTestcase : public content::RenderViewHostTestHarness {
 public:
  explicit PresentationServiceTestcase(
      const content::fuzzing::presentation_service::proto::Testcase& testcase);
  ~PresentationServiceTestcase() override;

  // Returns true once either all of the actions in the testcase have been
  // performed, or the per-testcase action limit has been exceeded.
  //
  // This should only be called from the fuzzer sequence.
  bool IsFinished();

  // If there are still actions remaining in the testcase, this will perform the
  // next sequence of actions before returning.
  //
  // If IsFinished() would return true, then calling this function is a no-op.
  //
  // This should only be called from the fuzzer sequence.
  void NextAction();

 private:
  using Action = content::fuzzing::presentation_service::proto::Action;

  void SetUp() override;
  void SetUpOnUIThread();

  void TearDown() override;
  void TearDownOnUIThread();

  // Create and bind a new instance for fuzzing. This needs to make sure that
  // the new instance has been created and bound on the correct sequence before
  // returning.
  void AddPresentationService(uint32_t id);

  void TestBody() override {}

  // The proto message describing the test actions to perform.
  const raw_ref<const content::fuzzing::presentation_service::proto::Testcase>
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

  // A fake delegate which we can control with protobuf messages,
  // the actions of which are also within our fuzzer's actions.
  // Required as `PresentationServiceDelegateImpl` expects UI interaction.
  std::unique_ptr<ControllerPresentationServiceDelegateForFuzzing>
      controller_delegate_;

  // Component which we fuzz
  std::unique_ptr<content::PresentationServiceImpl> presentation_service_;

  SEQUENCE_CHECKER(sequence_checker_);
};

PresentationServiceTestcase::PresentationServiceTestcase(
    const content::fuzzing::presentation_service::proto::Testcase& testcase)
    : RenderViewHostTestHarness(
          base::test::TaskEnvironment::TimeSource::MOCK_TIME,
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
          base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
          base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
          content::BrowserTaskEnvironment::REAL_IO_THREAD),
      testcase_(testcase) {
  SetUp();
}

PresentationServiceTestcase::~PresentationServiceTestcase() {
  TearDown();
}

bool PresentationServiceTestcase::IsFinished() {
  return next_sequence_idx_ >= testcase_->sequence_indexes_size();
}

void PresentationServiceTestcase::NextAction() {
  if (next_sequence_idx_ < testcase_->sequence_indexes_size()) {
    auto sequence_idx = testcase_->sequence_indexes(next_sequence_idx_++);
    const auto& sequence =
        testcase_->sequences(sequence_idx % testcase_->sequences_size());
    for (auto action_idx : sequence.action_indexes()) {
      if (!testcase_->actions_size() || ++action_count_ > max_action_count_) {
        return;
      }
      const auto& action =
          testcase_->actions(action_idx % testcase_->actions_size());
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

        case Action::kNewPresentationService: {
          AddPresentationService(action.new_presentation_service().id());
        } break;

        case Action::kPresentationServiceRemoteAction: {
          mojolpm::HandleRemoteAction(
              action.presentation_service_remote_action());
        } break;

        case Action::kPresentationControllerReceiverAction: {
          mojolpm::HandleReceiverAction(
              action.presentation_controller_receiver_action());
        } break;

        case Action::kPresentationReceiverReceiverAction: {
          mojolpm::HandleReceiverAction(
              action.presentation_receiver_receiver_action());
        } break;

        case Action::kControllerDelegateAction: {
          controller_delegate_->NextAction(action.controller_delegate_action());
        } break;

        case Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void PresentationServiceTestcase::SetUp() {
  RenderViewHostTestHarness::SetUp();

  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PresentationServiceTestcase::SetUpOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();
}

void PresentationServiceTestcase::SetUpOnUIThread() {
  content::TestRenderFrameHost* render_frame_host =
      static_cast<content::TestWebContents*>(web_contents())
          ->GetPrimaryMainFrame();
  render_frame_host->InitializeRenderFrameIfNeeded();

  presentation_service_ =
      content::PresentationServiceImpl::Create(render_frame_host);

  controller_delegate_ =
      std::make_unique<ControllerPresentationServiceDelegateForFuzzing>();
  presentation_service_->SetControllerDelegateForTesting(
      controller_delegate_.get());
}

void PresentationServiceTestcase::TearDown() {
  base::RunLoop run_loop;
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&PresentationServiceTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      run_loop.QuitClosure());
  run_loop.Run();

  RenderViewHostTestHarness::TearDown();
}

void PresentationServiceTestcase::TearDownOnUIThread() {
  presentation_service_.reset();
  controller_delegate_.reset();
}

void PresentationServiceTestcase::AddPresentationService(uint32_t id) {
  mojo::Remote<::blink::mojom::PresentationService> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  // `Unretained` is safe here, as `run_loop.Run()` blocks until
  // `PostTaskAndReply` calls the quit closure.
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&content::PresentationServiceImpl::Bind,
                     base::Unretained(presentation_service_.get()),
                     std::move(receiver)),
      run_loop.QuitClosure());
  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}

// Helper function to keep scheduling fuzzer actions on the current runloop
// until the testcase has completed, and then quit the runloop.
void NextAction(PresentationServiceTestcase* testcase,
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
void RunTestcase(PresentationServiceTestcase* testcase) {
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
    const content::fuzzing::presentation_service::proto::Testcase&
        proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  // Make sure that the environment is initialized before we do anything else.
  GetEnvironment();

  PresentationServiceTestcase testcase(proto_testcase);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Unretained is safe here, because ui_run_loop has to finish before testcase
  // goes out of scope.
  GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
