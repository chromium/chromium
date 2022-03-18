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
#include "base/no_destructor.h"
#include "base/test/test_switches.h"
#include "base/test/test_timeouts.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"  // nogncheck
#include "content/browser/file_system/file_system_manager_impl.h"  // nogncheck
#include "content/browser/storage_partition_impl_map.h"            // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_content_client_initializer.h"
#include "content/test/fuzzer/file_system_manager_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/core/embedder/embedder.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/quota/special_storage_policy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_context.h"
#include "testing/libfuzzer/proto/url_proto_converter.h"
#include "third_party/blink/public/common/storage_key/proto/storage_key.pb.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/common/storage_key/storage_key_proto_converter.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/gurl.h"
#include "url/origin.h"

using url::Origin;

namespace content {

const size_t kNumRenderers = 2;
const char* kCmdline[] = {"file_system_manager_mojolpm_fuzzer", nullptr};

mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<mojolpm::FuzzerEnvironmentWithTaskEnvironment>
      environment(1, kCmdline);
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
      const storage_key_proto::StorageKey& storage_key,
      mojo::PendingReceiver<::blink::mojom::FileSystemManager>&& receiver);

  // Create and bind a new instance for fuzzing. This needs to  make sure that
  // the new instance has been created and bound on the correct sequence before
  // returning.
  void AddFileSystemManager(
      uint32_t id,
      content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
          RenderProcessId render_process_id,
      const storage_key_proto::StorageKey& storage_key);

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
  GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::SetUpOnIOThread,
                     base::Unretained(this)),
      io_run_loop.QuitClosure());
  io_run_loop.Run();

  base::RunLoop ui_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
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
      temp_dir_.GetPath(), temp_dir_.GetPath(), GetIOThreadTaskRunner({}));
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
  content::GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::TearDownOnUIThread,
                     base::Unretained(this)),
      ui_run_loop.QuitClosure());
  ui_run_loop.Run();

  base::RunLoop io_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
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
              action.new_file_system_manager().render_process_id(),
              action.new_file_system_manager().storage_key());
          break;

        case Action::kRunThread:
          if (action.run_thread().id()) {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            GetUIThreadTaskRunner({})->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
            run_loop.Run();
          } else {
            base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
            GetIOThreadTaskRunner({})->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
            run_loop.Run();
          }
          break;

        case Action::kFileSystemManagerRemoteAction:
          ::mojolpm::HandleRemoteAction(
              action.file_system_manager_remote_action());
          break;

        case Action::kFileSystemCancellableOperationRemoteAction:
          ::mojolpm::HandleRemoteAction(
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
    const storage_key_proto::StorageKey& storage_key,
    mojo::PendingReceiver<::blink::mojom::FileSystemManager>&& receiver) {
  size_t offset = render_process_id ==
                          content::fuzzing::file_system_manager::proto::
                              NewFileSystemManagerAction_RenderProcessId_ZERO
                      ? 0
                      : 1;
  file_system_manager_impls_[offset]->BindReceiver(
      storage_key_proto::Convert(storage_key), std::move(receiver));
}

void FileSystemManagerTestcase::AddFileSystemManager(
    uint32_t id,
    content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
        RenderProcessId render_process_id,
    const storage_key_proto::StorageKey& storage_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<::blink::mojom::FileSystemManager> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::AddFileSystemManagerImpl,
                     base::Unretained(this), id, render_process_id, storage_key,
                     std::move(receiver)),
      run_loop.QuitClosure());
  run_loop.Run();

  ::mojolpm::GetContext()->AddInstance(id, std::move(remote));
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

  ::mojolpm::GetContext()->StartTestcase();

  base::RunLoop fuzzer_run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  content::GetFuzzerTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(NextAction, base::Unretained(testcase),
                                fuzzer_run_loop.QuitClosure()));
  fuzzer_run_loop.Run();

  ::mojolpm::GetContext()->EndTestcase();

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
