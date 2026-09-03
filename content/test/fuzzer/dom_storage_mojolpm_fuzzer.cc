// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/task/bind_post_task.h"
#include "components/services/storage/public/mojom/session_storage_control.mojom.h"  // nogncheck
#include "content/browser/dom_storage/dom_storage_context_wrapper.h"  // nogncheck
#include "content/browser/dom_storage/session_storage_namespace_handle_impl.h"  // nogncheck
#include "content/browser/security/cpsp/child_process_security_policy_impl.h"  // nogncheck
#include "content/browser/storage_partition_impl.h"  // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/dom_storage_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/dom_storage/session_storage_namespace.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/dom_storage/storage_area.mojom-mojolpm.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;
namespace dom_storage_proto = ::content::fuzzing::dom_storage::proto;

namespace {

// This fuzzer exercises the mojo interfaces exposed by the browser process to
// implement the DOM Storage web API in the renderer process. These interfaces
// are defined in //third_party/blink/public/mojom/dom_storage/.
//
// The StorageArea interface is the primary interface for reading and writing
// data to local storage and session storage.

const char* const kCmdline[] = {"dom_storage_mojolpm_fuzzer", nullptr};

constexpr int kRendererIdA = static_cast<int>(dom_storage_proto::RENDERER_A);
constexpr int kRendererIdB = static_cast<int>(dom_storage_proto::RENDERER_B);
constexpr content::ChildProcessId kRendererProcessA(kRendererIdA);
constexpr content::ChildProcessId kRendererProcessB(kRendererIdB);

blink::StorageKey GetStorageKeyForOriginId(
    dom_storage_proto::OriginId origin_id) {
  switch (origin_id) {
    case dom_storage_proto::ORIGIN_A:
      return blink::StorageKey::CreateFromStringForTesting(
          "https://example.com");
    case dom_storage_proto::ORIGIN_B:
      return blink::StorageKey::CreateFromStringForTesting("https://other.com");
    case dom_storage_proto::ORIGIN_OPAQUE:
      return blink::StorageKey::CreateFirstParty(Origin());
  }
  NOTREACHED();
}

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<
      content::mojolpm::FuzzerEnvironmentWithTaskEnvironment>
      environment(1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

}  // namespace

namespace content {

// Per-testcase state needed to run the interface being tested.
//
// The lifetime of this is scoped to a single testcase, and it is created and
// destroyed from the fuzzer sequence.
//
// For DOM Storage, this needs the basic common Browser process state provided
// by TestBrowserContext.
class DomStorageTestcase
    : public ::mojolpm::Testcase<fuzzing::dom_storage::proto::Testcase,
                                 fuzzing::dom_storage::proto::Action> {
 public:
  using ProtoTestcase = fuzzing::dom_storage::proto::Testcase;
  using ProtoAction = fuzzing::dom_storage::proto::Action;

  explicit DomStorageTestcase(const ProtoTestcase& testcase);

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);
  void SetUpOnFuzzerThread(base::OnceClosure done_closure);

  void TearDownOnUIThread(base::OnceClosure done_closure);
  void TearDownOnFuzzerThread(base::OnceClosure done_closure);

  StoragePartitionImpl* GetStoragePartition();
  // Called from the UI thread to open local storage.
  void OpenLocalStorage(
      int renderer_id,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  // Called from the UI thread to open session storage.
  void OpenSessionStorage(
      int renderer_id,
      const blink::StorageKey& storage_key,
      mojo::PendingReceiver<blink::mojom::StorageArea> receiver);

  // Called from the UI thread to bind the session storage namespace.
  void BindSessionStorageNamespace(
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver);

  // Helpers called from the fuzzer thread.
  void CreateAndAddLocalStorageArea(uint32_t id,
                                    const blink::StorageKey& storage_key,
                                    int renderer_id,
                                    base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void CreateAndAddSessionStorageArea(uint32_t id,
                                      const blink::StorageKey& storage_key,
                                      int renderer_id,
                                      base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Called from the UI thread; asks the wrapper to push pending writes through
  // to the storage service.
  void FlushStorageOnUIThread();

  void CreateAndAddSessionStorageNamespace(uint32_t id,
                                           base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Registers a pending target namespace via CloneFrom(immediately=false),
  // i.e. CloneNamespace with kWaitForCloneOnNamespace. The service holds the
  // target in pending-clone state until a later Clone() resolves it.
  void SetUpPendingClone(uint32_t id, base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void DoSetUpPendingClone(const std::string& target_namespace_id);

  // The target namespace is pending population, so the service queues this
  // receiver until the source namespace is cloned.
  void CreateAndAddTargetSessionStorageNamespace(uint32_t pending_clone_id,
                                                 uint32_t remote_id,
                                                 base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  void BindTargetSessionStorageNamespace(
      const std::string& target_namespace_id,
      mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver);

  // Clones the source namespace into a fresh id with kImmediate.
  void CloneNamespaceImmediate(base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DoCloneNamespaceImmediate();

  // Exercises the storage service's on-disk shallow-clone branch:
  // DeleteNamespace(should_persist=true) followed by CloneNamespace with
  // kWaitForCloneOnNamespace.
  void OnDiskClone(uint32_t id, base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DoOnDiskClone(const std::string& secondary_id,
                     const std::string& target_id);

  // Calls SessionStorageControl::PurgeMemory(), which iterates namespaces and
  // purges each namespace's unbound areas.
  void PurgeMemory(base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DoPurgeMemory();

  // Calls SessionStorageControl::DeleteStorage(), dispatching to
  // SessionStorageNamespaceImpl::RemoveStorageKeyData() for the namespace.
  void DeleteStorageKey(const blink::StorageKey& storage_key,
                        base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);
  void DoDeleteStorageKey(const blink::StorageKey& storage_key);

  std::unique_ptr<TestBrowserContext> browser_context_;
  blink::SessionStorageNamespaceId session_namespace_id_;
  scoped_refptr<SessionStorageNamespaceHandleImpl> session_namespace_;
  // Keeps alive cloned namespaces created via NewPendingCloneAction so that the
  // storage service retains their pending-clone state until Clone() is called.
  std::vector<scoped_refptr<SessionStorageNamespaceHandleImpl>>
      pending_clone_namespaces_;
};

DomStorageTestcase::DomStorageTestcase(
    const fuzzing::dom_storage::proto::Testcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase) {
  // DomStorageTestcase is created on the main thread, but the actions that
  // we want to validate the sequencing of take place on the fuzzer sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void DomStorageTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void DomStorageTestcase::SetUpOnUIThread(base::OnceClosure done_closure) {
  browser_context_ = std::make_unique<TestBrowserContext>();

  ChildProcessSecurityPolicyImpl* p =
      ChildProcessSecurityPolicyImpl::GetInstance();
  p->AddForTesting(kRendererProcessA, browser_context_.get());
  p->AddForTesting(kRendererProcessB, browser_context_.get());
  p->AddCommittedOrigin(kRendererIdA,
                        url::Origin::Create(GURL("https://example.com")));
  p->AddCommittedOrigin(kRendererIdB,
                        url::Origin::Create(GURL("https://other.com")));
  // Create a session storage namespace for session storage fuzzing.
  auto* dom_storage_context = GetStoragePartition()->GetDOMStorageContext();
  session_namespace_id_ = blink::AllocateSessionStorageNamespaceId();
  session_namespace_ = SessionStorageNamespaceHandleImpl::Create(
      dom_storage_context, session_namespace_id_);

  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::SetUpOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void DomStorageTestcase::SetUpOnFuzzerThread(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ::mojolpm::GetContext()->StartTestcase();
  std::move(done_closure).Run();
}

void DomStorageTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void DomStorageTestcase::TearDownOnUIThread(base::OnceClosure done_closure) {
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(kRendererProcessA);
  ChildProcessSecurityPolicyImpl::GetInstance()->Remove(kRendererProcessB);
  pending_clone_namespaces_.clear();
  session_namespace_.reset();
  browser_context_.reset();
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::TearDownOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void DomStorageTestcase::TearDownOnFuzzerThread(
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ::mojolpm::GetContext()->EndTestcase();
  std::move(done_closure).Run();
}

void DomStorageTestcase::RunAction(const ProtoAction& action,
                                   base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI = dom_storage_proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO = dom_storage_proto::RunThreadAction_ThreadId_IO;

  switch (action.action_case()) {
    case ProtoAction::kNewLocalStorageArea:
      CreateAndAddLocalStorageArea(
          action.new_local_storage_area().id(),
          GetStorageKeyForOriginId(action.new_local_storage_area().origin_id()),
          static_cast<int>(action.new_local_storage_area().renderer_id()),
          std::move(done_closure));
      return;

    case ProtoAction::kNewSessionStorageArea:
      CreateAndAddSessionStorageArea(
          action.new_session_storage_area().id(),
          GetStorageKeyForOriginId(
              action.new_session_storage_area().origin_id()),
          static_cast<int>(action.new_session_storage_area().renderer_id()),
          std::move(done_closure));
      return;

    case ProtoAction::kNewSessionStorageNamespace:
      CreateAndAddSessionStorageNamespace(
          action.new_session_storage_namespace().id(), std::move(done_closure));
      return;

    case ProtoAction::kNewPendingClone:
      SetUpPendingClone(action.new_pending_clone().id(),
                        std::move(done_closure));
      return;

    case ProtoAction::kNewTargetSessionStorageNamespace:
      CreateAndAddTargetSessionStorageNamespace(
          action.new_target_session_storage_namespace().id(),
          action.new_target_session_storage_namespace().remote_id(),
          std::move(done_closure));
      return;

    case ProtoAction::kCloneNamespaceImmediate:
      CloneNamespaceImmediate(std::move(done_closure));
      return;

    case ProtoAction::kOnDiskClone:
      OnDiskClone(action.on_disk_clone().id(), std::move(done_closure));
      return;

    case ProtoAction::kPurgeMemory:
      PurgeMemory(std::move(done_closure));
      return;

    case ProtoAction::kDeleteStorageKey:
      DeleteStorageKey(
          GetStorageKeyForOriginId(action.delete_storage_key().origin_id()),
          std::move(done_closure));
      return;

    case ProtoAction::kStorageAreaRemoteAction:
      ::mojolpm::HandleRemoteAction(action.storage_area_remote_action());
      break;

    case ProtoAction::kSessionStorageNamespaceRemoteAction:
      ::mojolpm::HandleRemoteAction(
          action.session_storage_namespace_remote_action());
      break;

    // These actions ensure that any tasks currently queued on the named
    // thread have a chance to run before the fuzzer continues.
    //
    // We don't provide any particular guarantees here; this does not mean
    // that the named thread is idle, nor does it prevent any other threads
    // from running (or the consequences of any resulting callbacks, for
    // example).
    case ProtoAction::kRunThread:
      if (action.run_thread().id() == ThreadId_UI) {
        GetUIThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(done_closure));
      } else if (action.run_thread().id() == ThreadId_IO) {
        GetIOThreadTaskRunner({})->PostTaskAndReply(
            FROM_HERE, base::DoNothing(), std::move(done_closure));
      }
      return;

    case ProtoAction::kFlushStorage:
      GetUIThreadTaskRunner({})->PostTaskAndReply(
          FROM_HERE,
          base::BindOnce(&DomStorageTestcase::FlushStorageOnUIThread,
                         base::Unretained(this)),
          std::move(done_closure));
      return;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

StoragePartitionImpl* DomStorageTestcase::GetStoragePartition() {
  return static_cast<StoragePartitionImpl*>(
      browser_context_->GetDefaultStoragePartition());
}

void DomStorageTestcase::OpenLocalStorage(
    int renderer_id,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  GetStoragePartition()->OpenLocalStorageForProcess(renderer_id, storage_key,
                                                    std::move(receiver));
}

void DomStorageTestcase::OpenSessionStorage(
    int renderer_id,
    const blink::StorageKey& storage_key,
    mojo::PendingReceiver<blink::mojom::StorageArea> receiver) {
  GetStoragePartition()->BindSessionStorageAreaForProcess(
      renderer_id, storage_key, session_namespace_id_, std::move(receiver));
}

void DomStorageTestcase::FlushStorageOnUIThread() {
  // Wrapper is null after Shutdown; the testcase tears it down only in
  // TearDownOnUIThread so reaching here without a context means setup failed.
  auto* wrapper = GetStoragePartition()->GetDOMStorageContext();
  if (wrapper) {
    wrapper->Flush();
  }
}

void DomStorageTestcase::BindSessionStorageNamespace(
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  // Bypass StoragePartitionImpl::BindSessionStorageNamespace. It calls
  // dom_storage_receivers_.GetBadMessageCallback(), which DCHECKs unless called
  // during a message dispatch on that receiver set. So we bind by directly
  // calling the wrapper instead.
  session_namespace_->context()->BindNamespace(
      session_namespace_id_, base::DoNothing(), std::move(receiver));
}

void DomStorageTestcase::CreateAndAddLocalStorageArea(
    uint32_t id,
    const blink::StorageKey& storage_key,
    int renderer_id,
    base::OnceClosure done_closure) {
  mojo::Remote<blink::mojom::StorageArea> storage_area_remote;
  auto storage_area_receiver = storage_area_remote.BindNewPipeAndPassReceiver();

  // Open local storage on the UI thread.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DomStorageTestcase::OpenLocalStorage,
                                base::Unretained(this), renderer_id,
                                storage_key, std::move(storage_area_receiver)));

  // Since the PendingReceiver is consumed asynchronously, flush the remote
  // before running done_closure.
  uint32_t lookup_id =
      ::mojolpm::GetContext()->AddInstance(id, std::move(storage_area_remote));
  ::mojolpm::GetContext()
      ->GetInstance<mojo::Remote<blink::mojom::StorageArea>>(lookup_id)
      ->FlushAsyncForTesting(
          base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::CreateAndAddSessionStorageArea(
    uint32_t id,
    const blink::StorageKey& storage_key,
    int renderer_id,
    base::OnceClosure done_closure) {
  mojo::Remote<blink::mojom::StorageArea> storage_area_remote;
  auto storage_area_receiver = storage_area_remote.BindNewPipeAndPassReceiver();

  // Open session storage on the UI thread.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&DomStorageTestcase::OpenSessionStorage,
                                base::Unretained(this), renderer_id,
                                storage_key, std::move(storage_area_receiver)));

  // Since the PendingReceiver is consumed asynchronously, flush the remote
  // before running done_closure.
  uint32_t lookup_id =
      ::mojolpm::GetContext()->AddInstance(id, std::move(storage_area_remote));
  ::mojolpm::GetContext()
      ->GetInstance<mojo::Remote<blink::mojom::StorageArea>>(lookup_id)
      ->FlushAsyncForTesting(
          base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::CreateAndAddSessionStorageNamespace(
    uint32_t id,
    base::OnceClosure done_closure) {
  mojo::Remote<blink::mojom::SessionStorageNamespace> namespace_remote;
  auto namespace_receiver = namespace_remote.BindNewPipeAndPassReceiver();

  // Bind the session storage namespace on the UI thread.
  GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::BindSessionStorageNamespace,
                     base::Unretained(this), std::move(namespace_receiver)));

  // Since the PendingReceiver is consumed asynchronously, flush the remote
  // before running done_closure.
  uint32_t lookup_id =
      ::mojolpm::GetContext()->AddInstance(id, std::move(namespace_remote));
  ::mojolpm::GetContext()
      ->GetInstance<mojo::Remote<blink::mojom::SessionStorageNamespace>>(
          lookup_id)
      ->FlushAsyncForTesting(
          base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::SetUpPendingClone(uint32_t id,
                                           base::OnceClosure done_closure) {
  std::string target_id =
      base::StringPrintf("%08x_0000_0000_0000_%012x", id, id);
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::DoSetUpPendingClone,
                     base::Unretained(this), std::move(target_id)),
      base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::DoSetUpPendingClone(
    const std::string& target_namespace_id) {
  pending_clone_namespaces_.push_back(
      SessionStorageNamespaceHandleImpl::CloneFrom(
          scoped_refptr<DOMStorageContextWrapper>(
              session_namespace_->context()),
          target_namespace_id, session_namespace_id_,
          /*immediately=*/false));
}

void DomStorageTestcase::BindTargetSessionStorageNamespace(
    const std::string& target_namespace_id,
    mojo::PendingReceiver<blink::mojom::SessionStorageNamespace> receiver) {
  session_namespace_->context()->BindNamespace(
      target_namespace_id, base::DoNothing(), std::move(receiver));
}

void DomStorageTestcase::CreateAndAddTargetSessionStorageNamespace(
    uint32_t pending_clone_id,
    uint32_t remote_id,
    base::OnceClosure done_closure) {
  std::string target_id = base::StringPrintf(
      "%08x_0000_0000_0000_%012x", pending_clone_id, pending_clone_id);

  mojo::Remote<blink::mojom::SessionStorageNamespace> namespace_remote;
  auto namespace_receiver = namespace_remote.BindNewPipeAndPassReceiver();

  ::mojolpm::GetContext()->AddInstance(remote_id, std::move(namespace_remote));
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::BindTargetSessionStorageNamespace,
                     base::Unretained(this), std::move(target_id),
                     std::move(namespace_receiver)),
      base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::CloneNamespaceImmediate(
    base::OnceClosure done_closure) {
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::DoCloneNamespaceImmediate,
                     base::Unretained(this)),
      base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::DoCloneNamespaceImmediate() {
  pending_clone_namespaces_.push_back(session_namespace_->Clone());
}

void DomStorageTestcase::OnDiskClone(uint32_t id,
                                     base::OnceClosure done_closure) {
  std::string secondary_id =
      base::StringPrintf("%08x_0001_0001_0001_%012x", id, id);
  std::string target_id =
      base::StringPrintf("%08x_0002_0002_0002_%012x", id, id);
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::DoOnDiskClone, base::Unretained(this),
                     std::move(secondary_id), std::move(target_id)),
      base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::DoOnDiskClone(const std::string& secondary_id,
                                       const std::string& target_id) {
  storage::mojom::SessionStorageControl* control =
      session_namespace_->context()->GetSessionStorageControl();

  control->CreateNamespace(secondary_id);

  mojo::Remote<blink::mojom::SessionStorageNamespace> temp_remote;
  control->BindNamespace(secondary_id,
                         temp_remote.BindNewPipeAndPassReceiver());

  control->DeleteNamespace(secondary_id, /*should_persist=*/true);

  control->CloneNamespace(
      secondary_id, target_id,
      storage::mojom::SessionStorageCloneType::kWaitForCloneOnNamespace);
}

void DomStorageTestcase::PurgeMemory(base::OnceClosure done_closure) {
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::DoPurgeMemory,
                     base::Unretained(this)),
      base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::DoPurgeMemory() {
  session_namespace_->context()->GetSessionStorageControl()->PurgeMemory();
}

void DomStorageTestcase::DeleteStorageKey(const blink::StorageKey& storage_key,
                                          base::OnceClosure done_closure) {
  GetUIThreadTaskRunner({})->PostTaskAndReply(
      FROM_HERE,
      base::BindOnce(&DomStorageTestcase::DoDeleteStorageKey,
                     base::Unretained(this), storage_key),
      base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

void DomStorageTestcase::DoDeleteStorageKey(
    const blink::StorageKey& storage_key) {
  // StorageKey::Serialize() DCHECKs on opaque origins.
  if (storage_key.origin().opaque()) {
    return;
  }

  session_namespace_->context()->GetSessionStorageControl()->DeleteStorage(
      storage_key, session_namespace_id_, base::DoNothing());
}

}  // namespace content

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::dom_storage::proto::Testcase& proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  GetEnvironment();

  content::DomStorageTestcase testcase(proto_testcase);

  // Unretained is safe here, because `main_run_loop` has to finish before
  // testcase goes out of scope.
  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<content::DomStorageTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}
