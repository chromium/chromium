// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/342213636): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/no_destructor.h"
#include "base/task/sequenced_task_runner.h"
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
#include "mojo/public/tools/fuzzers/mojolpm.h"
#include "storage/browser/file_system/file_permission_policy.h"
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
class FileSystemManagerTestcase
    : public ::mojolpm::Testcase<
          content::fuzzing::file_system_manager::proto::Testcase,
          content::fuzzing::file_system_manager::proto::Action> {
 public:
  using ProtoTestcase = content::fuzzing::file_system_manager::proto::Testcase;
  using ProtoAction = content::fuzzing::file_system_manager::proto::Action;

  explicit FileSystemManagerTestcase(
      const content::fuzzing::file_system_manager::proto::Testcase& testcase);

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnIOThread(base::OnceClosure done_closure);
  void SetUpOnUIThread(base::OnceClosure done_closure);
  void TearDownOnIOThread(base::OnceClosure done_closure);
  void TearDownOnUIThread(base::OnceClosure done_closure);

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
      const storage_key_proto::StorageKey& storage_key,
      base::OnceClosure done_closure);

  // Prerequisite state
  TestBrowserContext browser_context_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  scoped_refptr<ChromeBlobStorageContext> blob_storage_context_;

  // Mapping from renderer id to FileSystemManagerImpl instances being fuzzed.
  // Access only from UI thread.
  std::unique_ptr<FileSystemManagerImpl>
      file_system_manager_impls_[kNumRenderers];
};

FileSystemManagerTestcase::FileSystemManagerTestcase(
    const content::fuzzing::file_system_manager::proto::Testcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase), browser_context_() {
  // FileSystemManagerTestcase is created on the main thread, but the actions
  // that we want to validate the sequencing of take place on the fuzzer
  // sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void FileSystemManagerTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::SetUpOnIOThread,
                     base::Unretained(this), std::move(done_closure)));
}

void FileSystemManagerTestcase::SetUpOnIOThread(
    base::OnceClosure done_closure) {
  CHECK(temp_dir_.CreateUniqueTempDir());
  file_system_context_ =
      storage::CreateFileSystemContextForTesting(nullptr, temp_dir_.GetPath());

  blob_storage_context_ = base::MakeRefCounted<ChromeBlobStorageContext>();
  blob_storage_context_->InitializeOnIOThread(
      temp_dir_.GetPath(), temp_dir_.GetPath(), GetIOThreadTaskRunner({}));

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void FileSystemManagerTestcase::SetUpOnUIThread(
    base::OnceClosure done_closure) {
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

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void FileSystemManagerTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void FileSystemManagerTestcase::TearDownOnIOThread(
    base::OnceClosure done_closure) {
  for (size_t i = 0; i < kNumRenderers; i++) {
    file_system_manager_impls_[i].reset();
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void FileSystemManagerTestcase::TearDownOnUIThread(
    base::OnceClosure done_closure) {
  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  for (size_t i = 0; i < kNumRenderers; i++) {
    p->Remove(i);
  }

  GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::TearDownOnIOThread,
                     base::Unretained(this), std::move(done_closure)));
}

void FileSystemManagerTestcase::RunAction(const ProtoAction& action,
                                          base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI =
      content::fuzzing::file_system_manager::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::file_system_manager::proto::RunThreadAction_ThreadId_IO;

  switch (action.action_case()) {
    case ProtoAction::kNewFileSystemManager:
      AddFileSystemManager(action.new_file_system_manager().id(),
                           action.new_file_system_manager().render_process_id(),
                           action.new_file_system_manager().storage_key(),
                           std::move(run_closure));
      return;

    case ProtoAction::kRunThread:
      if (action.run_thread().id() == ThreadId_UI) {
        GetUIThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      } else if (action.run_thread().id() == ThreadId_IO) {
        content::GetIOThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(run_closure));
      }
      return;

    case ProtoAction::kFileSystemManagerRemoteAction:
      ::mojolpm::HandleRemoteAction(action.file_system_manager_remote_action());
      break;

    case ProtoAction::kFileSystemCancellableOperationRemoteAction:
      ::mojolpm::HandleRemoteAction(
          action.file_system_cancellable_operation_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(run_closure));
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

static void AddFileSystemManagerInstance(
    uint32_t id,
    mojo::Remote<::blink::mojom::FileSystemManager> remote,
    base::OnceClosure run_closure) {
  ::mojolpm::GetContext()->AddInstance(id, std::move(remote));

  std::move(run_closure).Run();
}

void FileSystemManagerTestcase::AddFileSystemManager(
    uint32_t id,
    content::fuzzing::file_system_manager::proto::NewFileSystemManagerAction::
        RenderProcessId render_process_id,
    const storage_key_proto::StorageKey& storage_key,
    base::OnceClosure run_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojo::Remote<::blink::mojom::FileSystemManager> remote;
  auto receiver = remote.BindNewPipeAndPassReceiver();

  GetIOThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&FileSystemManagerTestcase::AddFileSystemManagerImpl,
                     base::Unretained(this), id, render_process_id, storage_key,
                     std::move(receiver)),
      base::BindOnce(&AddFileSystemManagerInstance, id, std::move(remote),
                     std::move(run_closure)));
}
}  // namespace content

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

  base::RunLoop main_run_loop;

  // Unretained is safe here, because `main_run_loop` has to finish before
  // testcase goes out of scope.
  content::GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<content::FileSystemManagerTestcase>,
                     base::Unretained(&testcase),
                     content::GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));

  main_run_loop.Run();
}
