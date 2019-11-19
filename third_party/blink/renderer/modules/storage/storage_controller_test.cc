// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_controller.h"

#include "base/bind.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/modules/storage/testing/mock_storage_area.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/uuid.h"

namespace blink {
namespace {

const size_t kTestCacheLimit = 100;
class MockStoragePartitionService
    : public mojom::blink::StoragePartitionService {
 public:
  void OpenLocalStorage(
      const scoped_refptr<const SecurityOrigin>& origin,
      mojo::PendingReceiver<mojom::blink::StorageArea> receiver) override {}

  void OpenSessionStorage(
      const String& namespace_id,
      mojo::PendingReceiver<mojom::blink::SessionStorageNamespace> receiver)
      override {
    session_storage_opens++;
  }

  void GetSessionStorageUsage(int32_t* out) const {
    *out = session_storage_opens;
  }

  int32_t session_storage_opens = 0;
};

}  // namespace

TEST(StorageControllerTest, CacheLimit) {
  const auto kOrigin = SecurityOrigin::CreateFromString("http://dom_storage1/");
  const auto kOrigin2 =
      SecurityOrigin::CreateFromString("http://dom_storage2/");
  const auto kOrigin3 =
      SecurityOrigin::CreateFromString("http://dom_storage3/");
  const String kKey("key");
  const String kValue("value");
  const KURL kPageUrl("http://dom_storage/page");
  Persistent<FakeAreaSource> source_area =
      MakeGarbageCollected<FakeAreaSource>(kPageUrl);

  mojo::PendingRemote<mojom::blink::StoragePartitionService>
      storage_partition_service_remote;
  PostCrossThreadTask(
      *base::CreateSequencedTaskRunner({base::ThreadPool()}), FROM_HERE,
      CrossThreadBindOnce(
          [](mojo::PendingReceiver<mojom::blink::StoragePartitionService>
                 receiver) {
            mojo::MakeSelfOwnedReceiver(
                std::make_unique<MockStoragePartitionService>(),
                std::move(receiver));
          },
          WTF::Passed(storage_partition_service_remote
                          .InitWithNewPipeAndPassReceiver())));

  StorageController controller(scheduler::GetSingleThreadTaskRunnerForTesting(),
                               std::move(storage_partition_service_remote),
                               kTestCacheLimit);

  auto cached_area1 = controller.GetLocalStorageArea(kOrigin.get());
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);
  const auto* area1_ptr = cached_area1.get();
  size_t expected_total = (kKey.length() + kValue.length()) * 2;
  EXPECT_EQ(expected_total, cached_area1->quota_used());
  EXPECT_EQ(expected_total, controller.TotalCacheSize());
  cached_area1 = nullptr;

  auto cached_area2 = controller.GetLocalStorageArea(kOrigin2.get());
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);
  // Area for kOrigin should still be alive.
  EXPECT_EQ(2 * cached_area2->quota_used(), controller.TotalCacheSize());
  EXPECT_EQ(area1_ptr, controller.GetLocalStorageArea(kOrigin.get()));

  String long_value(Vector<UChar>(kTestCacheLimit, 'a'));
  cached_area2->SetItem(kKey, long_value, source_area);
  // Cache is cleared when a new area is opened.
  auto cached_area3 = controller.GetLocalStorageArea(kOrigin3.get());
  EXPECT_EQ(cached_area2->quota_used(), controller.TotalCacheSize());
}

TEST(StorageControllerTest, CacheLimitSessionStorage) {
  const String kNamespace1 = WTF::CreateCanonicalUUIDString();
  const String kNamespace2 = WTF::CreateCanonicalUUIDString();
  const auto kOrigin = SecurityOrigin::CreateFromString("http://dom_storage1/");
  const auto kOrigin2 =
      SecurityOrigin::CreateFromString("http://dom_storage2/");
  const auto kOrigin3 =
      SecurityOrigin::CreateFromString("http://dom_storage3/");
  const String kKey("key");
  const String kValue("value");
  const KURL kPageUrl("http://dom_storage/page");

  Persistent<FakeAreaSource> source_area =
      MakeGarbageCollected<FakeAreaSource>(kPageUrl);

  auto task_runner = base::CreateSequencedTaskRunner({base::ThreadPool()});

  auto mock_storage_partition_service =
      std::make_unique<MockStoragePartitionService>();
  MockStoragePartitionService* storage_partition_ptr =
      mock_storage_partition_service.get();

  mojo::PendingRemote<mojom::blink::StoragePartitionService>
      storage_partition_service_remote;
  PostCrossThreadTask(
      *task_runner, FROM_HERE,
      CrossThreadBindOnce(
          [](std::unique_ptr<MockStoragePartitionService> storage_partition_ptr,
             mojo::PendingReceiver<mojom::blink::StoragePartitionService>
                 receiver) {
            mojo::MakeSelfOwnedReceiver(std::move(storage_partition_ptr),
                                        std::move(receiver));
          },
          WTF::Passed(std::move(mock_storage_partition_service)),
          WTF::Passed(storage_partition_service_remote
                          .InitWithNewPipeAndPassReceiver())));

  StorageController controller(
      nullptr, std::move(storage_partition_service_remote), kTestCacheLimit);

  StorageNamespace* ns1 = controller.CreateSessionStorageNamespace(kNamespace1);
  StorageNamespace* ns2 = controller.CreateSessionStorageNamespace(kNamespace2);

  auto cached_area1 = ns1->GetCachedArea(kOrigin.get());
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);
  const auto* area1_ptr = cached_area1.get();
  size_t expected_total = (kKey.length() + kValue.length()) * 2;
  EXPECT_EQ(expected_total, cached_area1->quota_used());
  EXPECT_EQ(expected_total, controller.TotalCacheSize());
  cached_area1 = nullptr;

  auto cached_area2 = ns2->GetCachedArea(kOrigin2.get());
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);
  // Area for kOrigin should still be alive.
  EXPECT_EQ(2 * cached_area2->quota_used(), controller.TotalCacheSize());
  EXPECT_EQ(area1_ptr, ns1->GetCachedArea(kOrigin.get()));

  String long_value(Vector<UChar>(kTestCacheLimit, 'a'));
  cached_area2->SetItem(kKey, long_value, source_area);
  // Cache is cleared when a new area is opened.
  auto cached_area3 = ns1->GetCachedArea(kOrigin3.get());
  EXPECT_EQ(cached_area2->quota_used(), controller.TotalCacheSize());

  int32_t opens = 0;
  {
    base::RunLoop loop;
    task_runner->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&MockStoragePartitionService::GetSessionStorageUsage,
                       base::Unretained(storage_partition_ptr), &opens),
        loop.QuitClosure());
    loop.Run();
  }
  EXPECT_EQ(opens, 2);
}

}  // namespace blink
