// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/indexed_db_test_base.h"

#include <set>
#include <utility>

#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/test_future.h"
#include "components/services/storage/privileged/cpp/bucket_client_info.h"
#include "components/services/storage/public/cpp/buckets/bucket_locator.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/indexed_db/instance/leveldb/indexed_db_leveldb_operations.h"
#include "third_party/blink/public/mojom/quota/quota_types.mojom-shared.h"

namespace content::indexed_db {

namespace {

base::FilePath CreateAndReturnTempDir(base::ScopedTempDir* temp_dir) {
  CHECK(temp_dir->CreateUniqueTempDir());
  return temp_dir->GetPath();
}

}  // namespace

IndexedDBTestBase::IndexedDBTestBase(bool use_default_buckets, bool use_sqlite)
    : sqlite_override_(
          BucketContext::OverrideShouldUseSqliteForTesting(use_sqlite)),
      use_default_buckets_(use_default_buckets),
      use_sqlite_(use_sqlite),
      special_storage_policy_(
          base::MakeRefCounted<storage::MockSpecialStoragePolicy>()),
      quota_manager_(base::MakeRefCounted<storage::MockQuotaManager>(
          /*is_incognito=*/false,
          CreateAndReturnTempDir(&temp_dir_),
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          special_storage_policy_)),
      quota_manager_proxy_(base::MakeRefCounted<storage::MockQuotaManagerProxy>(
          quota_manager_.get(),
          base::SequencedTaskRunner::GetCurrentDefault())) {
  mojo::PendingRemote<storage::mojom::BlobStorageContext>
      pending_blob_storage_context;
  blob_storage_context_.Clone(
      pending_blob_storage_context.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
  file_system_access_context_.Clone(
      fsa_context.InitWithNewPipeAndPassReceiver());

  context_ = std::make_unique<IndexedDBContextImpl>(
      temp_dir_.GetPath(), quota_manager_proxy_.get(),
      std::move(pending_blob_storage_context), std::move(fsa_context),
      base::SequencedTaskRunner::GetCurrentDefault());
  // Let the mojo pipes be bound before proceeding. See
  // IndexedDBContextImpl::BindPipesOnIDBSequence().
  RunPostedTasks();
}

IndexedDBTestBase::~IndexedDBTestBase() = default;

void IndexedDBTestBase::SetUp() {
  ResetGlobalSweepAndCompactionTimesForTest();
}

void IndexedDBTestBase::TearDown() {
  factory_remote_.reset();

  if (context_ && !context_->in_memory()) {
    std::set<storage::BucketLocator> buckets = context_->bucket_set_;
    base::RunLoop run_loop;
    auto barrier = base::IgnoreArgs<blink::mojom::QuotaStatusCode>(
        base::BarrierClosure(buckets.size(), run_loop.QuitClosure()));
    for (const storage::BucketLocator& bucket_locator : buckets) {
      context_->DeleteBucketData(bucket_locator, barrier);
    }
    run_loop.Run();
  }

  EXPECT_TRUE(temp_dir_.Delete());
}

void IndexedDBTestBase::SetUpInMemoryContext() {
  mojo::PendingRemote<storage::mojom::BlobStorageContext>
      pending_blob_storage_context;
  blob_storage_context_.Clone(
      pending_blob_storage_context.InitWithNewPipeAndPassReceiver());
  mojo::PendingRemote<storage::mojom::FileSystemAccessContext> fsa_context;
  file_system_access_context_.Clone(
      fsa_context.InitWithNewPipeAndPassReceiver());
  context_ = std::make_unique<IndexedDBContextImpl>(
      base::FilePath(), quota_manager_proxy_.get(),
      std::move(pending_blob_storage_context), std::move(fsa_context),
      base::SequencedTaskRunner::GetCurrentDefault());
  // The mojo pipes are bound asynchronously, and must be bound before
  // proceeding with testing.
  RunPostedTasks();
}

void IndexedDBTestBase::RunPostedTasks(
    std::optional<storage::BucketLocator> bucket_locator) {
  base::RunLoop loop;
  if (bucket_locator) {
    context_->FlushBucketSequenceForTesting(*bucket_locator,
                                            loop.QuitClosure());
  } else {
    context_->idb_task_runner()->PostTask(FROM_HERE, loop.QuitClosure());
  }
  loop.Run();
}

storage::BucketInfo IndexedDBTestBase::GetOrCreateBucket(
    const storage::BucketInitParams& params) {
  base::test::TestFuture<storage::QuotaErrorOr<storage::BucketInfo>> future;
  quota_manager_proxy_->UpdateOrCreateBucket(
      params, base::SingleThreadTaskRunner::GetCurrentDefault(),
      future.GetCallback());
  return future.Take().value();
}

storage::BucketInfo IndexedDBTestBase::InitBucket(
    const blink::StorageKey& storage_key) {
  return GetOrCreateBucket(
      UseDefaultBuckets()
          ? storage::BucketInitParams::ForDefaultBucket(storage_key)
          : storage::BucketInitParams(storage_key, "non_default"));
}

BucketContext& IndexedDBTestBase::InitBucketContext(
    const blink::StorageKey& storage_key) {
  storage::BucketInfo bucket_info = InitBucket(storage_key);
  return GetOrCreateBucketContext(
      bucket_info, context()->GetDataPath(bucket_info.ToBucketLocator()));
}

void IndexedDBTestBase::BindFactory(
    mojo::PendingRemote<storage::mojom::IndexedDBClientStateChecker>
        checker_remote,
    mojo::PendingReceiver<blink::mojom::IDBFactory> receiver,
    storage::QuotaErrorOr<storage::BucketInfo> bucket_info) {
  context()->BindIndexedDBImpl(storage::BucketClientInfo{},
                               std::move(checker_remote), std::move(receiver),
                               bucket_info);
}

blink::StorageKey IndexedDBTestBase::GetTestStorageKey() {
  return blink::StorageKey::CreateFromStringForTesting("http://test/");
}

BucketContext& IndexedDBTestBase::GetOrCreateBucketContext(
    const storage::BucketInfo& bucket,
    const base::FilePath& data_directory) {
  context_->EnsureBucketContext(bucket, data_directory);
  return *GetBucketContext(bucket.id);
}

BucketContext* IndexedDBTestBase::GetBucketContext(storage::BucketId id) {
  base::SequenceBound<BucketContext>* sequence_bound =
      context_->GetBucketContextForTesting(id);
  if (!sequence_bound) {
    return nullptr;
  }
  base::test::TestFuture<BucketContext*> future;
  sequence_bound->AsyncCall(&BucketContext::GetReferenceForTesting)
      .Then(future.GetCallback());
  return future.Get();
}

BucketContextHandle IndexedDBTestBase::CreateBucketHandle() {
  BucketContext& bucket_context = InitBucketContext(GetTestStorageKey());
  BucketContextHandle bucket_context_handle(bucket_context);
  bucket_context_handle->InitBackingStore(/*create_if_missing=*/true);
  return bucket_context_handle;
}

}  // namespace content::indexed_db
