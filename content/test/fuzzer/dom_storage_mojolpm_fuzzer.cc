// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/task/bind_post_task.h"
#include "content/browser/child_process_security_policy_impl.h"  // nogncheck
#include "content/browser/dom_storage/session_storage_namespace_impl.h"  // nogncheck
#include "content/browser/storage_partition_impl.h"              // nogncheck
#include "content/public/browser/browser_task_traits.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/dom_storage_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/common/dom_storage/session_storage_namespace_id.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
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

  std::unique_ptr<TestBrowserContext> browser_context_;
  blink::SessionStorageNamespaceId session_namespace_id_;
  scoped_refptr<SessionStorageNamespaceImpl> session_namespace_;
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
  session_namespace_ = SessionStorageNamespaceImpl::Create(
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

    case ProtoAction::kStorageAreaRemoteAction: {
      ::mojolpm::HandleRemoteAction(action.storage_area_remote_action());
      break;
    }

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
