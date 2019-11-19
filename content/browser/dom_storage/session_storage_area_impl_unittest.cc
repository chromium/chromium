// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_area_impl.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "content/browser/dom_storage/session_storage_data_map.h"
#include "content/browser/dom_storage/session_storage_metadata.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/test/gmock_util.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

MATCHER(OKStatus, "Equality matcher for type OK leveldb::Status") {
  return arg.ok();
}

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() = default;
  ~MockListener() override = default;
  MOCK_METHOD2(OnDataMapCreation,
               void(const std::vector<uint8_t>& map_id,
                    SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(const std::vector<uint8_t>& map_id));
  MOCK_METHOD1(OnCommitResult, void(leveldb::Status));
};

class SessionStorageAreaImplTest : public testing::Test {
 public:
  SessionStorageAreaImplTest()
      : test_namespace_id1_(base::GenerateGUID()),
        test_namespace_id2_(base::GenerateGUID()),
        test_origin1_(url::Origin::Create(GURL("https://host1.com:1/"))),
        test_origin2_(url::Origin::Create(GURL("https://host2.com:2/"))) {
    leveldb_database_ = storage::AsyncDomStorageDatabase::OpenInMemory(
        base::nullopt, "SessionStorageAreaImplTestDatabase",
        base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()}),
        base::DoNothing());
    leveldb_database_->Put(StdStringToUint8Vector("map-0-key1"),
                           StdStringToUint8Vector("data1"), base::DoNothing());

    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask>
        save_tasks = metadata_.SetupNewDatabase();
    auto map_id = metadata_.RegisterNewMap(
        metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
        &save_tasks);
    DCHECK(map_id->KeyPrefix() == StdStringToUint8Vector("map-0-"));
    leveldb_database_->RunBatchDatabaseTasks(std::move(save_tasks),
                                             base::DoNothing());
  }
  ~SessionStorageAreaImplTest() override = default;

  scoped_refptr<SessionStorageMetadata::MapData> RegisterNewAreaMap(
      SessionStorageMetadata::NamespaceEntry namespace_entry,
      const url::Origin& origin) {
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto map_data =
        metadata_.RegisterNewMap(namespace_entry, origin, &save_tasks);
    leveldb_database_->RunBatchDatabaseTasks(std::move(save_tasks),
                                             base::DoNothing());
    return map_data;
  }

  SessionStorageAreaImpl::RegisterNewAreaMap GetRegisterNewAreaMapCallback() {
    return base::BindRepeating(&SessionStorageAreaImplTest::RegisterNewAreaMap,
                               base::Unretained(this));
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  const std::string test_namespace_id1_;
  const std::string test_namespace_id2_;
  const url::Origin test_origin1_;
  const url::Origin test_origin2_;
  std::unique_ptr<storage::AsyncDomStorageDatabase> leveldb_database_;
  SessionStorageMetadata metadata_;

  testing::StrictMock<MockListener> listener_;
};
}  // namespace

TEST_F(SessionStorageAreaImplTest, BasicUsage) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb;
  ss_leveldb_impl->Bind(
      ss_leveldb.BindNewEndpointAndPassDedicatedReceiverForTesting());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(ss_leveldb.get(), &data));
  ASSERT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
}

TEST_F(SessionStorageAreaImplTest, ExplicitlyEmptyMap) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateEmpty(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb;
  ss_leveldb_impl->Bind(
      ss_leveldb.BindNewEndpointAndPassDedicatedReceiverForTesting());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(ss_leveldb.get(), &data));
  ASSERT_EQ(0ul, data.size());

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
}

TEST_F(SessionStorageAreaImplTest, DoubleBind) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb1;
  base::RunLoop loop;
  ss_leveldb_impl->Bind(
      ss_leveldb1.BindNewEndpointAndPassDedicatedReceiverForTesting());
  ss_leveldb1.set_disconnect_handler(loop.QuitClosure());
  // Check that we can bind twice and get data from the second binding.
  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb2;
  ss_leveldb_impl->Bind(
      ss_leveldb2.BindNewEndpointAndPassDedicatedReceiverForTesting());
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(ss_leveldb2.get(), &data));
  ASSERT_EQ(1ul, data.size());

  // Make sure the first binding was closed.
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
}

TEST_F(SessionStorageAreaImplTest, Cloning) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl1 = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  // Perform a shallow clone.
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  metadata_.RegisterShallowClonedNamespace(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_), &save_tasks);
  leveldb_database_->RunBatchDatabaseTasks(std::move(save_tasks),
                                           base::DoNothing());
  auto ss_leveldb_impl2 = ss_leveldb_impl1->Clone(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_));

  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb1;
  ss_leveldb_impl1->Bind(
      ss_leveldb1.BindNewEndpointAndPassDedicatedReceiverForTesting());
  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb2;
  ss_leveldb_impl2->Bind(
      ss_leveldb2.BindNewEndpointAndPassDedicatedReceiverForTesting());

  // Same maps are used.
  EXPECT_EQ(ss_leveldb_impl1->data_map(), ss_leveldb_impl2->data_map());

  // The |Put| call will fork the maps.
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(testing::AnyNumber());
  EXPECT_TRUE(test::PutSync(ss_leveldb2.get(), StdStringToUint8Vector("key2"),
                            StdStringToUint8Vector("data2"), base::nullopt,
                            ""));

  // The maps were forked on the above put.
  EXPECT_NE(ss_leveldb_impl1->data_map(), ss_leveldb_impl2->data_map());

  // Check map 1 data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(ss_leveldb1.get(), &data));
  ASSERT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  // Check map 2 data.
  data.clear();
  EXPECT_TRUE(test::GetAllSync(ss_leveldb2.get(), &data));
  ASSERT_EQ(2ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  ss_leveldb_impl1 = nullptr;
  ss_leveldb_impl2 = nullptr;
}

TEST_F(SessionStorageAreaImplTest, NotifyAllDeleted) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl1 = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb1;
  ss_leveldb_impl1->Bind(
      ss_leveldb1.BindNewEndpointAndPassDedicatedReceiverForTesting());

  testing::StrictMock<test::MockLevelDBObserver> mock_observer;
  mojo::AssociatedReceiver<blink::mojom::StorageAreaObserver> observer_receiver(
      &mock_observer);
  ss_leveldb1->AddObserver(observer_receiver.BindNewEndpointAndPassRemote());
  ss_leveldb1.FlushForTesting();

  base::RunLoop loop;
  EXPECT_CALL(mock_observer, AllDeleted("\n"))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  ss_leveldb_impl1->NotifyObserversAllDeleted();
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
}

TEST_F(SessionStorageAreaImplTest, DeleteAllOnShared) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl1 = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  // Perform a shallow clone.
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  metadata_.RegisterShallowClonedNamespace(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_), &save_tasks);
  leveldb_database_->RunBatchDatabaseTasks(std::move(save_tasks),
                                           base::DoNothing());
  auto ss_leveldb_impl2 = ss_leveldb_impl1->Clone(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_));

  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb1;
  ss_leveldb_impl1->Bind(
      ss_leveldb1.BindNewEndpointAndPassDedicatedReceiverForTesting());
  mojo::AssociatedRemote<blink::mojom::StorageArea> ss_leveldb2;
  ss_leveldb_impl2->Bind(
      ss_leveldb2.BindNewEndpointAndPassDedicatedReceiverForTesting());

  // Same maps are used.
  EXPECT_EQ(ss_leveldb_impl1->data_map(), ss_leveldb_impl2->data_map());

  // Create the observer, attach to the first namespace, and verify we don't see
  // any changes (see SessionStorageAreaImpl class comment about when observers
  // are called).
  testing::StrictMock<test::MockLevelDBObserver> mock_observer;
  mojo::AssociatedReceiver<blink::mojom::StorageAreaObserver> observer_receiver(
      &mock_observer);
  ss_leveldb1->AddObserver(observer_receiver.BindNewEndpointAndPassRemote());

  // The |DeleteAll| call will fork the maps, and the observer should see a
  // DeleteAll.
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  // There should be no commits, as we don't actually have to change any data.
  // |ss_leveldb_impl1| should just switch to a new, empty map.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(0);
  EXPECT_TRUE(test::DeleteAllSync(ss_leveldb1.get(), "source"));

  // The maps were forked on the above call.
  EXPECT_NE(ss_leveldb_impl1->data_map(), ss_leveldb_impl2->data_map());

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  ss_leveldb_impl1 = nullptr;
  ss_leveldb_impl2 = nullptr;
}

TEST_F(SessionStorageAreaImplTest, DeleteAllWithoutBinding) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl1 = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  base::RunLoop loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  EXPECT_TRUE(test::DeleteAllSync(ss_leveldb_impl1.get(), "source"));
  ss_leveldb_impl1->data_map()->storage_area()->ScheduleImmediateCommit();
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);

  ss_leveldb_impl1 = nullptr;
}

TEST_F(SessionStorageAreaImplTest, DeleteAllWithoutBindingOnShared) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  auto ss_leveldb_impl1 = std::make_unique<SessionStorageAreaImpl>(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_), test_origin1_,
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_)
              ->second[test_origin1_],
          leveldb_database_.get()),
      GetRegisterNewAreaMapCallback());

  // Perform a shallow clone.
  std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
  metadata_.RegisterShallowClonedNamespace(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_), &save_tasks);
  leveldb_database_->RunBatchDatabaseTasks(std::move(save_tasks),
                                           base::DoNothing());
  auto ss_leveldb_impl2 = ss_leveldb_impl1->Clone(
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id2_));

  // Same maps are used.
  EXPECT_EQ(ss_leveldb_impl1->data_map(), ss_leveldb_impl2->data_map());

  // The |DeleteAll| call will fork the maps, and the observer should see a
  // DeleteAll.
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  // There should be no commits, as we don't actually have to change any data.
  // |ss_leveldb_impl1| should just switch to a new, empty map.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(0);
  EXPECT_TRUE(test::DeleteAllSync(ss_leveldb_impl1.get(), "source"));

  // The maps were forked on the above call.
  EXPECT_NE(ss_leveldb_impl1->data_map(), ss_leveldb_impl2->data_map());

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  ss_leveldb_impl1 = nullptr;
  ss_leveldb_impl2 = nullptr;
}

}  // namespace content
