// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <utility>

#include "base/at_exit.h"
#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/files/scoped_temp_dir.h"
#include "base/i18n/icu_util.h"
#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"  // nogncheck
#include "content/browser/file_system/file_system_manager_impl.h"  // nogncheck
#include "content/browser/storage_partition_impl_map.h"            // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/fuzzer/file_system_manager_mojolpm_fuzzer.pb.h"
#include "mojo/core/embedder/embedder.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;

namespace content {

const size_t kNumRenderers = 2;
const char* cmdline[] = {"file_system_manager_mojolpm_fuzzer", nullptr};

// Global environment needed to run the interface being tested.
//
// This will be created once, before fuzzing starts, and will be shared between
// all testcases. It is created on the main thread.
//
// At a minimum, we should always be able to set up the command line, i18n and
// mojo, and create the thread on which the fuzzer will be run. We want to avoid
// (as much as is reasonable) any state being preserved between testcases.
//
// For FileSystemManager, we can also safely re-use a single
// BrowserTaskEnvironment and the TestContentClientInitializer between
// testcases. We try to create an environment that matches the real browser
// process as much as possible, so we use real platform threads in the task
// environment.
class ContentFuzzerEnvironment {
 public:
  ContentFuzzerEnvironment()
      : fuzzer_thread_("fuzzer_thread"),
        task_environment_(
            (base::CommandLine::Init(1, cmdline),
             TestTimeouts::Initialize(),
             base::test::TaskEnvironment::MainThreadType::DEFAULT),
            base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC,
            base::test::TaskEnvironment::ThreadingMode::MULTIPLE_THREADS,
            content::BrowserTaskEnvironment::REAL_IO_THREAD) {
    logging::SetMinLogLevel(logging::LOG_FATAL);
    mojo::core::Init();
    base::i18n::InitializeICU();
    fuzzer_thread_.StartAndWaitForTesting();
  }

  scoped_refptr<base::SequencedTaskRunner> fuzzer_task_runner() {
    return fuzzer_thread_.task_runner();
  }

 private:
  base::AtExitManager at_exit_manager_;
  base::Thread fuzzer_thread_;
  content::BrowserTaskEnvironment task_environment_;
  content::TestContentClientInitializer content_client_initializer_;
};

ContentFuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<ContentFuzzerEnvironment> environment;
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
// For FileSystemManager, this needs the basic common Browser process state
// provided by TestBrowserContext, and to set up the storage contexts that will
// be used. The filesystem APIs also depend on the Blob subsystem, so in
// addition to the FileSystemContext we also need a BlobStorageContext.
//
// Since the Browser process will host one FileSystemManagerImpl per
// RenderProcessHost, we emulate this by allowing the fuzzer to create (and
// destroy) multiple FileSystemManagerImpl instances.
class FileSystemManagerTestcase {
 public:
  explicit FileSystemManagerTestcase(
      const content::fuzzing::file_system_manager::proto::Testcase& testcase);

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

  void SetUp();
  void TearDown();

 private:
  using Action = content::fuzzing::file_system_manager::proto::Action;

  void SetUpOnIOThread();
  void SetUpOnUIThread();
  void TearDownOnIOThread();
  void TearDownOnUIThread();

  // Used by AddFileSystemManager to create and bind FileSystemManagerImpl on the
  // UI thread.
  void AddFileSystemManagerImpl(
      uint32_t id,
      content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
          RenderProcessId render_process_id,
      mojo::PendingReceiver<::blink::mojom::FileSystemManager>&& receiver);

  // Create and bind a new instance for fuzzing. This needs to  make sure that
  // the new instance has been created and bound on the correct sequence before
  // returning.
  void AddFileSystemManager(
      uint32_t id,
      content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
          RenderProcessId render_process_id);

  // The proto message describing the test actions to perform.
  const content::fuzzing::file_system_manager::proto::Testcase& testcase_;

  // Apply a reasonable upper-bound on testcase complexity to avoid timeouts.
  const int max_action_count_ = 512;

  // Count of total actions performed in this testcase.
  int action_count_ = 0;

  // The index of the next sequence of actions to execute.
  int next_sequence_idx_ = 0;

  // Prerequisite state
  TestBrowserContext browser_context_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Mapping from renderer id to FileSystemManagerImpl instances being fuzzed.
  // Access only from UI thread.
  std::unique_ptr<FileSystemManagerImpl>
      file_system_manager_impls_[kNumRenderers];

  SEQUENCE_CHECKER(sequence_checker_);
};

FileSystemManagerTestcase::FileSystemManagerTestcase(
    const content::fuzzing::file_system_manager::proto::Testcase& testcase)
    : testcase_(testcase), browser_context_() {
  // FileSystemManagerTestcase is created on the main thread, but the actions
  // that we want to validate the sequencing of take place on the fuzzer
  // sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void FileSystemManagerTestcase::SetUp() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::RunLoop io_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&FileSystemManagerTestcase::SetUpOnIOThread,
                     base::Unretained(this)),
      io_run_loop.QuitClosure());
  io_run_loop.Run();

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {BrowserThread::UI},
      base::BindOnce(&FileSystemManagerTestcase::SetUpOnUIThread,
                     base::Unretained(this)),
      ui_run_loop.QuitClosure());
  ui_run_loop.Run();
}

void FileSystemManagerTestcase::SetUpOnIOThread() {
  CHECK(temp_dir_.CreateUniqueTempDir());
  file_system_context_ =
      storage::CreateFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

  blob_storage_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
  blob_storage_context_->InitializeOnIOThread(
      temp_dir_.GetPath(), temp_dir_.GetPath(),
      base::CreateSingleThreadTaskRunner({BrowserThread::IO}));
}

void FileSystemManagerTestcase::SetUpOnUIThread() {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->RegisterFileSystemPermissionPolicy(storage::kFileSystemTypeTest,
                                        storage::FILE_PERMISSION_SANDBOX);
  p->RegisterFileSystemPermissionPolicy(storage::kFileSystemTypeTemporary,
                                        storage::FILE_PERMISSION_SANDBOX);

  // Note - FileSystemManagerImpl must be constructed on the UI thread, but all
  // other methods are expected to be called on the IO thread - see comments in
  // content/browser/file_system/file_system_manager_impl.h
  for (size_t i = 0; i < kNumRenderers; i++) {
    file_system_manager_impls_[i] = std::make_unique<FileSystemManagerImpl>(
        i, file_system_context_, blob_storage_context_);
    p->Add(i, &browser_context_);
  }
}

void FileSystemManagerTestcase::TearDown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::UI},
      base::BindOnce(&FileSystemManagerTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      ui_run_loop.QuitClosure());
  ui_run_loop.Run();

  base::RunLoop io_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&FileSystemManagerTestcase::TearDownOnIOThread,
                     base::Unretained(this)),
      io_run_loop.QuitClosure());
  io_run_loop.Run();
}

void FileSystemManagerTestcase::TearDownOnIOThread() {
  for (size_t i = 0; i < kNumRenderers; i++) {
    file_system_manager_impls_[i].reset();
  }
}

void FileSystemManagerTestcase::TearDownOnUIThread() {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  for (size_t i = 0; i < kNumRenderers; i++) {
    p->Remove(i);
  }
}

bool FileSystemManagerTestcase::IsFinished() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return next_sequence_idx_ >= testcase_.sequence_indexes_size();
}

void FileSystemManagerTestcase::NextAction() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
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
      switch (action.action_case()) {
        case Action::kNewFileSystemManager:
          AddFileSystemManager(
              action.new_file_system_manager().id(),
              action.new_file_system_manager().render_process_id());
          break;

        case Action::kRunThread:
          if (action.run_thread().id()) {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            base::PostTask(FROM_HERE, {BrowserThread::UI},
                           run_loop.QuitClosure());
            run_loop.Run();
          } else {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            base::PostTask(FROM_HERE, {BrowserThread::IO},
                           run_loop.QuitClosure());
            run_loop.Run();
          }
          break;

        case Action::kFileSystemManagerRemoteAction:
          mojolpm::HandleRemoteAction(
              action.file_system_manager_remote_action());
          break;

        case Action::kFileSystemCancellableOperationRemoteAction:
          mojolpm::HandleRemoteAction(
              action.file_system_cancellable_operation_remote_action());
          break;

        case Action::ACTION_NOT_SET:
          break;
      }
    }
  }
}

void FileSystemManagerTestcase::AddFileSystemManagerImpl(
    uint32_t id,
    content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
        RenderProcessId render_process_id,
    mojo::PendingReceiver<::blink::mojom::FileSystemManager>&& receiver) {
  size_t offset = render_process_id ==
                          content::fuzzing::file_system_manager::proto::
                              NewFileSystemManagerAction_RenderProcessId_ZERO
                      ? 0
                      : 1;
  file_system_manager_impls_[offset]->BindReceiver(std::move(receiver));
}

void FileSystemManagerTestcase::AddFileSystemManager(
    uint32_t id,
    content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
        RenderProcessId render_process_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<::blink::mojom::FileSystemManager> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  base::PostTaskAndReply(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&FileSystemManagerTestcase::AddFileSystemManagerImpl,
                     base::Unretained(this), id, render_process_id,
                     std::move(receiver)),
      run_loop.QuitClosure());
  run_loop.Run();

  mojolpm::GetContext()->AddInstance(id, std::move(remote));
}
}  // namespace content

// Helper function to keep scheduling fuzzer actions on the current runloop
// until the testcase has completed, and then quit the runloop.
void NextAction(content::FileSystemManagerTestcase* testcase,
                base::RepeatingClosure quit_closure) {
  if (!testcase->IsFinished()) {
    testcase->NextAction();
    content::GetFuzzerTaskRunner()->PostTask(
        FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                  std::move(quit_closure)));
  } else {
    content::GetFuzzerTaskRunner()->PostTask(FROM_HERE,
                                             std::move(quit_closure));
  }
}

// Helper function to setup and run the testcase, since we need to do that from
// the fuzzer sequence rather than the main thread.
void RunTestcase(content::FileSystemManagerTestcase* testcase) {
  mojo::Message message;
  auto dispatch_context =
      std::make_unique<mojo::internal::MessageDispatchContext>(&message);

  testcase->SetUp();

  mojolpm::GetContext()->StartTestcase();

  base::RunLoop fuzzer_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  fuzzer_run_loop.Run();

  mojolpm::GetContext()->EndTestcase();

  testcase->TearDown();
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::file_system_manager::proto::Testcase&
        proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  // Make sure that the environment is initialized before we do anything else.
  content::GetEnvironment();

  content::FileSystemManagerTestcase testcase(proto_testcase);

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);

  // Unretained is safe here, because ui_run_loop has to finish before testcase
  // goes out of scope.
  content::GetFuzzerTaskRunner()->PostTaskAndReply(
      FROM_HERE, base::BindOnce(RunTestcase, base::Unretained(&testcase)),
      ui_run_loop.QuitClosure());

  ui_run_loop.Run();
}
