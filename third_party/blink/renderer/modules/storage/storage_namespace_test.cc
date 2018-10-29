// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/storage/storage_namespace.h"
#include <third_party/blink/renderer/modules/storage/storage_controller.h>

#include "base/task/post_task.h"
#include "base/test/scoped_feature_list.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/scheduler/test/fake_renderer_scheduler.h"
#include "third_party/blink/renderer/modules/storage/testing/fake_area_source.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/uuid.h"
#include "third_party/blink/renderer/platform/web_task_runner.h"

namespace blink {
namespace {
class NoopStoragePartitionService
    : public mojom::blink::StoragePartitionService {
 public:
  void OpenLocalStorage(const scoped_refptr<const SecurityOrigin>& origin,
                        mojom::blink::StorageAreaRequest request) override {}

  void OpenSessionStorage(
      const String& namespace_id,
      mojom::blink::SessionStorageNamespaceRequest request) override {}
};

}  // namespace

class StorageNamespaceTest : public testing::Test {
 public:
  const size_t kTestCacheLimit = 100;

  StorageNamespaceTest() {
    features_.InitAndEnableFeature(features::kOnionSoupDOMStorage);
  }
  ~StorageNamespaceTest() override {}

  base::test::ScopedFeatureList features_;
};

TEST_F(StorageNamespaceTest, BasicStorageAreas) {
  const auto kOrigin = SecurityOrigin::CreateFromString("http://dom_storage1/");
  const auto kOrigin2 =
      SecurityOrigin::CreateFromString("http://dom_storage2/");
  const auto kOrigin3 =
      SecurityOrigin::CreateFromString("http://dom_storage3/");
  const String kKey("key");
  const String kValue("value");
  const String kSessionStorageNamespace("abcd");
  const KURL kPageUrl("http://dom_storage/page");
  Persistent<FakeAreaSource> source_area = new FakeAreaSource(kPageUrl);

  blink::scheduler::FakeRendererScheduler renderer_scheduler;

  mojom::blink::StoragePartitionServicePtr storage_partition_service_ptr;
  PostCrossThreadTask(
      *base::CreateSequencedTaskRunnerWithTraits({}), FROM_HERE,
      CrossThreadBind(
          [](mojom::blink::StoragePartitionServiceRequest request) {
            mojo::MakeStrongBinding(
                std::make_unique<NoopStoragePartitionService>(),
                std::move(request));
          },
          WTF::Passed(MakeRequest(&storage_partition_service_ptr))));

  StorageController controller(renderer_scheduler.IPCTaskRunner(),
                               std::move(storage_partition_service_ptr),
                               kTestCacheLimit);
  StorageNamespace* localStorage = new StorageNamespace(&controller);
  StorageNamespace* sessionStorage =
      new StorageNamespace(&controller, kSessionStorageNamespace);

  EXPECT_FALSE(localStorage->IsSessionStorage());
  EXPECT_TRUE(sessionStorage->IsSessionStorage());

  auto cached_area1 = localStorage->GetCachedArea(kOrigin.get());
  cached_area1->RegisterSource(source_area);
  cached_area1->SetItem(kKey, kValue, source_area);
  auto cached_area2 = localStorage->GetCachedArea(kOrigin2.get());
  cached_area2->RegisterSource(source_area);
  cached_area2->SetItem(kKey, kValue, source_area);
  auto cached_area3 = sessionStorage->GetCachedArea(kOrigin3.get());
  cached_area3->RegisterSource(source_area);
  cached_area3->SetItem(kKey, kValue, source_area);

  EXPECT_EQ(cached_area1->GetItem(kKey), kValue);
  EXPECT_EQ(cached_area2->GetItem(kKey), kValue);
  EXPECT_EQ(cached_area3->GetItem(kKey), kValue);
}

}  // namespace blink
