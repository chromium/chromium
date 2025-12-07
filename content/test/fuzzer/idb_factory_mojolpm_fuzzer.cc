// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <utility>

#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/privileged/mojom/indexed_db_client_state_checker.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control.mojom.h"
#include "components/services/storage/privileged/mojom/indexed_db_control_test.mojom.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "content/browser/indexed_db/instance/bucket_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/fuzzer/idb_factory_mojolpm_fuzzer.pb.h"
#include "content/test/fuzzer/mojolpm_fuzzer_support.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom-mojolpm.h"
#include "third_party/blink/public/mojom/indexeddb/indexeddb.mojom.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_macro.h"
#include "url/origin.h"

using url::Origin;

// This fuzzer exercises the mojo interfaces exposed by the browser process to
// implement the IndexedDB web API in the renderer process. These interfaces are
// defined in //third_party/blink/public/mojom/indexeddb/indexeddb.mojom. The
// IDBFactory interface is the entry point for all IndexedDB operations.

const char* const kCmdline[] = {"idb_factory_mojolpm_fuzzer", nullptr};

content::mojolpm::FuzzerEnvironment& GetEnvironment() {
  static base::NoDestructor<
      content::mojolpm::FuzzerEnvironmentWithTaskEnvironment>
      environment(1, kCmdline);
  return *environment;
}

scoped_refptr<base::SequencedTaskRunner> GetFuzzerTaskRunner() {
  return GetEnvironment().fuzzer_task_runner();
}

// TODO(crbug.com/448235811): Hook up the actual state checker since it lives in
// the browser process.
class MockIndexedDBClientStateChecker
    : public storage::mojom::IndexedDBClientStateChecker {
 public:
  MockIndexedDBClientStateChecker() = default;
  ~MockIndexedDBClientStateChecker() override = default;

  // storage::mojom::IndexedDBClientStateChecker:
  void DisallowInactiveClient(
      int32_t connection_id,
      storage::mojom::DisallowInactiveClientReason reason,
      mojo::PendingReceiver<storage::mojom::IndexedDBClientKeepActive>
          keep_active,
      storage::mojom::IndexedDBClientStateChecker::
          DisallowInactiveClientCallback callback) override {}
  void MakeClone(
      mojo::PendingReceiver<storage::mojom::IndexedDBClientStateChecker>
          checker) override {}
};

// Per-testcase state needed to run the interface being tested.
//
// The lifetime of this is scoped to a single testcase, and it is created and
// destroyed from the fuzzer sequence.
//
// For IDBFactory, this needs the basic common Browser process state provided
// by TestBrowserContext, and the storage service.
//
// Since the Browser process will host one IDBFactory per bucket, we emulate
// this by allowing the fuzzer to create (and destroy) multiple IDBFactory
// instances.
class IdbFactoryTestcase
    : public mojolpm::Testcase<content::fuzzing::idb_factory::proto::Testcase,
                               content::fuzzing::idb_factory::proto::Action> {
 public:
  using ProtoTestcase = content::fuzzing::idb_factory::proto::Testcase;
  using ProtoAction = content::fuzzing::idb_factory::proto::Action;

  explicit IdbFactoryTestcase(const ProtoTestcase& testcase);

  void SetUp(base::OnceClosure done_closure) override;
  void TearDown(base::OnceClosure done_closure) override;

  void RunAction(const ProtoAction& action,
                 base::OnceClosure done_closure) override;

 private:
  void SetUpOnUIThread(base::OnceClosure done_closure);
  void SetUpOnFuzzerThread(base::OnceClosure done_closure);

  void TearDownOnUIThread(base::OnceClosure done_closure);
  void TearDownOnFuzzerThread(base::OnceClosure done_closure);

  // These are called from the UI thread.
  void FlushBucketSequence(base::OnceClosure done_closure);
  void BindIndexedDB(
      storage::BucketClientInfo client_info,
      mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
          checker_remote,
      mojo::PendingReceiver<blink::mojom::IDBFactory> receiver);

  // Helpers called from the fuzzer thread.
  void CreateAndAddIdbFactory(uint32_t id, base::OnceClosure done_closure)
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  const bool in_memory_;
  const bool use_sqlite_;
  const base::AutoReset<std::optional<bool>> sqlite_override_;
  const storage::BucketLocator bucket_locator_;

  // These are set up on the UI thread.
  std::unique_ptr<content::TestBrowserContext> browser_context_;
  mojo::Remote<storage::mojom::IndexedDBControlTest> indexed_db_control_test_;

  // These live on the fuzzer thread for now.
  MockIndexedDBClientStateChecker mock_client_state_checker_;
  mojo::ReceiverSet<storage::mojom::IndexedDBClientStateChecker>
      checker_receivers_ GUARDED_BY_CONTEXT(sequence_checker_);
};

IdbFactoryTestcase::IdbFactoryTestcase(
    const content::fuzzing::idb_factory::proto::Testcase& testcase)
    : Testcase<ProtoTestcase, ProtoAction>(testcase),
      in_memory_(testcase.in_memory()),
      use_sqlite_(testcase.use_sqlite()),
      sqlite_override_(
          content::indexed_db::BucketContext::OverrideShouldUseSqliteForTesting(
              use_sqlite_)),
      bucket_locator_(storage::BucketLocator::ForDefaultBucket(
          blink::StorageKey::CreateFirstParty(
              Origin::Create(GURL("https://example.com"))))) {
  // IdbFactoryTestcase is created on the main thread, but the actions that
  // we want to validate the sequencing of take place on the fuzzer sequence.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

void IdbFactoryTestcase::SetUp(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IdbFactoryTestcase::SetUpOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void IdbFactoryTestcase::SetUpOnUIThread(base::OnceClosure done_closure) {
  browser_context_ = std::make_unique<content::TestBrowserContext>();
  browser_context_->set_is_off_the_record(in_memory_);
  browser_context_->GetDefaultStoragePartition()
      ->GetIndexedDBControl()
      .BindTestInterfaceForTesting(
          indexed_db_control_test_.BindNewPipeAndPassReceiver());
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdbFactoryTestcase::SetUpOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void IdbFactoryTestcase::SetUpOnFuzzerThread(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  mojolpm::GetContext()->StartTestcase();
  std::move(done_closure).Run();
}

void IdbFactoryTestcase::TearDown(base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IdbFactoryTestcase::TearDownOnUIThread,
                     base::Unretained(this), std::move(done_closure)));
}

void IdbFactoryTestcase::TearDownOnUIThread(base::OnceClosure done_closure) {
  browser_context_.reset();
  indexed_db_control_test_.reset();
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&IdbFactoryTestcase::TearDownOnFuzzerThread,
                     base::Unretained(this), std::move(done_closure)));
}

void IdbFactoryTestcase::TearDownOnFuzzerThread(
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  checker_receivers_.Clear();
  mojolpm::GetContext()->EndTestcase();
  std::move(done_closure).Run();
}

void IdbFactoryTestcase::RunAction(const ProtoAction& action,
                                   base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto ThreadId_UI =
      content::fuzzing::idb_factory::proto::RunThreadAction_ThreadId_UI;
  const auto ThreadId_IO =
      content::fuzzing::idb_factory::proto::RunThreadAction_ThreadId_IO;
  const auto ThreadId_Bucket =
      content::fuzzing::idb_factory::proto::RunThreadAction_ThreadId_Bucket;

  switch (action.action_case()) {
    case ProtoAction::kRunThread:
      // These actions ensure that any tasks currently queued on the named
      // thread have chance to run before the fuzzer continues.
      //
      // We don't provide any particular guarantees here; this does not mean
      // that the named thread is idle, nor does it prevent any other threads
      // from running (or the consequences of any resulting callbacks, for
      // example).
      switch (action.run_thread().id()) {
        case ThreadId_UI:
          content::GetUIThreadTaskRunner({})->PostTaskAndReply(
              FROM_HERE, base::DoNothing(), std::move(done_closure));
          break;
        case ThreadId_IO:
          content::GetIOThreadTaskRunner({})->PostTaskAndReply(
              FROM_HERE, base::DoNothing(), std::move(done_closure));
          break;
        case ThreadId_Bucket:
          content::GetUIThreadTaskRunner({})->PostTask(
              FROM_HERE,
              base::BindOnce(&IdbFactoryTestcase::FlushBucketSequence,
                             base::Unretained(this),
                             base::BindPostTask(GetFuzzerTaskRunner(),
                                                std::move(done_closure))));
          break;
      }
      return;

    case ProtoAction::kNewIdbFactory:
      CreateAndAddIdbFactory(action.new_idb_factory().id(),
                             std::move(done_closure));
      return;

    case ProtoAction::kIdbFactoryRemoteAction:
      mojolpm::HandleRemoteAction(action.idb_factory_remote_action());
      break;

    case ProtoAction::kIdbDatabaseAssociatedRemoteAction:
      mojolpm::HandleAssociatedRemoteAction(
          action.idb_database_associated_remote_action());
      break;

    case ProtoAction::kIdbTransactionAssociatedRemoteAction:
      mojolpm::HandleAssociatedRemoteAction(
          action.idb_transaction_associated_remote_action());
      break;

    case ProtoAction::ACTION_NOT_SET:
      break;
  }

  GetFuzzerTaskRunner()->PostTask(FROM_HERE, std::move(done_closure));
}

void IdbFactoryTestcase::FlushBucketSequence(base::OnceClosure done_closure) {
  indexed_db_control_test_->FlushBucketSequenceForTesting(
      bucket_locator_, std::move(done_closure));
}

void IdbFactoryTestcase::BindIndexedDB(
    storage::BucketClientInfo client_info,
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver) {
  browser_context_->GetDefaultStoragePartition()
      ->GetIndexedDBControl()
      .BindIndexedDB(bucket_locator_, std::move(client_info),
                     std::move(checker_remote), std::move(receiver));
}

void IdbFactoryTestcase::CreateAndAddIdbFactory(
    uint32_t id,
    base::OnceClosure done_closure) {
  // TODO(crbug.com/448235811): Expand to diverse bucket types and clients.
  storage::BucketClientInfo client_info;
  mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
      checker_remote;
  checker_receivers_.Add(&mock_client_state_checker_,
                         checker_remote.InitWithNewPipeAndPassReceiver(),
                         GetFuzzerTaskRunner());

  mojo::Remote<blink::mojom::IDBFactory> factory_remote;
  auto factory_receiver = factory_remote.BindNewPipeAndPassReceiver();

  // Use of Unretained is safe since `this` is guaranteed to live at least until
  // `done_closure` is invoked.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&IdbFactoryTestcase::BindIndexedDB, base::Unretained(this),
                     std::move(client_info), std::move(checker_remote),
                     std::move(factory_receiver)));

  // Since the `PendingReceiver` to `IDBFactory` is consumed asynchronously by
  // `BindIndexedDB`, flush the remote before running `done_closure`.
  //
  // Note that `done_closure` may synchronously delete the remote (if the next
  // action is a reset of `IDBFactory`, for instance). To prevent a
  // use-after-free inside `FlushAsyncForTesting`, post a task to run
  // `done_closure` when the flush completes instead of running it directly.
  uint32_t lookup_id =
      mojolpm::GetContext()->AddInstance(id, std::move(factory_remote));
  mojolpm::GetContext()
      ->GetInstance<mojo::Remote<blink::mojom::IDBFactory>>(lookup_id)
      ->FlushAsyncForTesting(
          base::BindPostTask(GetFuzzerTaskRunner(), std::move(done_closure)));
}

DEFINE_BINARY_PROTO_FUZZER(
    const content::fuzzing::idb_factory::proto::Testcase& proto_testcase) {
  if (!proto_testcase.actions_size() || !proto_testcase.sequences_size() ||
      !proto_testcase.sequence_indexes_size()) {
    return;
  }

  GetEnvironment();

  IdbFactoryTestcase testcase(proto_testcase);

  // Unretained is safe here, because `main_run_loop` has to finish before
  // testcase goes out of scope.
  base::RunLoop main_run_loop;
  GetFuzzerTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&mojolpm::RunTestcase<IdbFactoryTestcase>,
                     base::Unretained(&testcase), GetFuzzerTaskRunner(),
                     main_run_loop.QuitClosure()));
  main_run_loop.Run();
}
