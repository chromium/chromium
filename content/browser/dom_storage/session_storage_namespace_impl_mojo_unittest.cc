// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_data_map.h"
#include "content/browser/dom_storage/session_storage_metadata.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/test/fake_leveldb_database.h"
#include "content/test/gmock_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {
namespace {
using leveldb::StdStringToUint8Vector;
using leveldb::mojom::DatabaseError;
using NamespaceEntry = SessionStorageMetadata::NamespaceEntry;

constexpr const int kTestProcessIdOrigin1 = 11;
constexpr const int kTestProcessIdAllOrigins = 12;
constexpr const int kTestProcessIdOrigin3 = 13;

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() {}
  ~MockListener() override {}
  MOCK_METHOD2(OnDataMapCreation,
               void(const std::vector<uint8_t>& map_id,
                    SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(const std::vector<uint8_t>& map_id));
  MOCK_METHOD1(OnCommitResult, void(leveldb::mojom::DatabaseError error));
};

class SessionStorageNamespaceImplMojoTest
    : public testing::Test,
      public SessionStorageNamespaceImplMojo::Delegate {
 public:
  SessionStorageNamespaceImplMojoTest()
      : test_namespace_id1_(base::GenerateGUID()),
        test_namespace_id2_(base::GenerateGUID()),
        test_origin1_(url::Origin::Create(GURL("https://host1.com:1/"))),
        test_origin2_(url::Origin::Create(GURL("https://host2.com:2/"))),
        test_origin3_(url::Origin::Create(GURL("https://host3.com:3/"))),
        database_(&mock_data_) {}
  ~SessionStorageNamespaceImplMojoTest() override = default;

  void SetUp() override {
    // Create a database that already has a namespace saved.
    metadata_.SetupNewDatabase();
    std::vector<leveldb::mojom::BatchedOperationPtr> save_operations;
    auto entry = metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);
    auto map_id =
        metadata_.RegisterNewMap(entry, test_origin1_, &save_operations);
    DCHECK(map_id->KeyPrefix() == StdStringToUint8Vector("map-0-"));
    database_.Write(std::move(save_operations), base::DoNothing());
    // Put some data in one of the maps.
    mock_data_[StdStringToUint8Vector("map-0-key1")] =
        StdStringToUint8Vector("data1");

    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Add(kTestProcessIdOrigin1);
    security_policy->Add(kTestProcessIdAllOrigins);
    security_policy->Add(kTestProcessIdOrigin3);
    security_policy->AddIsolatedOrigins(
        {test_origin1_, test_origin2_, test_origin3_});
    security_policy->LockToOrigin(kTestProcessIdOrigin1,
                                  test_origin1_.GetURL());
    security_policy->LockToOrigin(kTestProcessIdOrigin3,
                                  test_origin3_.GetURL());

    mojo::core::SetDefaultProcessErrorCallback(
        base::BindRepeating(&SessionStorageNamespaceImplMojoTest::OnBadMessage,
                            base::Unretained(this)));
  }

  void OnBadMessage(const std::string& reason) { bad_message_called_ = true; }

  void TearDown() override {
    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Remove(kTestProcessIdOrigin1);
    security_policy->Remove(kTestProcessIdAllOrigins);
    security_policy->Remove(kTestProcessIdOrigin3);

    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  // Creates a SessionStorageNamespaceImplMojo, saves it in the namespaces_ map,
  // and returns a pointer to the object.
  SessionStorageNamespaceImplMojo* CreateSessionStorageNamespaceImplMojo(
      const std::string& namespace_id) {
    DCHECK(namespaces_.find(namespace_id) == namespaces_.end());
    SessionStorageAreaImpl::RegisterNewAreaMap map_id_callback =
        base::BindRepeating(
            &SessionStorageNamespaceImplMojoTest::RegisterNewAreaMap,
            base::Unretained(this));

    auto namespace_impl = std::make_unique<SessionStorageNamespaceImplMojo>(
        namespace_id, &listener_, std::move(map_id_callback), this);
    auto* namespace_impl_ptr = namespace_impl.get();
    namespaces_[namespace_id] = std::move(namespace_impl);
    return namespace_impl_ptr;
  }

  scoped_refptr<SessionStorageMetadata::MapData> RegisterNewAreaMap(
      NamespaceEntry namespace_entry,
      const url::Origin& origin) {
    std::vector<leveldb::mojom::BatchedOperationPtr> save_operations;
    auto map_data =
        metadata_.RegisterNewMap(namespace_entry, origin, &save_operations);
    database_.Write(std::move(save_operations), base::DoNothing());
    return map_data;
  }

  void RegisterShallowClonedNamespace(
      NamespaceEntry source_namespace,
      const std::string& destination_namespace,
      const SessionStorageNamespaceImplMojo::OriginAreas& areas_to_clone)
      override {
    std::vector<leveldb::mojom::BatchedOperationPtr> save_operations;
    auto namespace_entry =
        metadata_.GetOrCreateNamespaceEntry(destination_namespace);
    metadata_.RegisterShallowClonedNamespace(source_namespace, namespace_entry,
                                             &save_operations);
    database_.Write(std::move(save_operations), base::DoNothing());

    auto it = namespaces_.find(destination_namespace);
    if (it == namespaces_.end()) {
      auto* namespace_impl =
          CreateSessionStorageNamespaceImplMojo(destination_namespace);
      namespace_impl->PopulateAsClone(&database_, namespace_entry,
                                      areas_to_clone);
      return;
    }
    it->second->PopulateAsClone(&database_, namespace_entry, areas_to_clone);
  }

  scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
      const std::vector<uint8_t>& map_number_as_bytes) override {
    auto it = data_maps_.find(map_number_as_bytes);
    if (it == data_maps_.end())
      return nullptr;
    return it->second;
  }

 protected:
  TestBrowserThreadBundle test_browser_thread_bundle_;
  const std::string test_namespace_id1_;
  const std::string test_namespace_id2_;
  const url::Origin test_origin1_;
  const url::Origin test_origin2_;
  const url::Origin test_origin3_;
  SessionStorageMetadata metadata_;
  bool bad_message_called_ = false;

  std::map<std::string, std::unique_ptr<SessionStorageNamespaceImplMojo>>
      namespaces_;
  std::map<std::vector<uint8_t>, scoped_refptr<SessionStorageDataMap>>
      data_maps_;

  testing::StrictMock<MockListener> listener_;
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> mock_data_;
  FakeLevelDBDatabase database_;
};

TEST_F(SessionStorageNamespaceImplMojoTest, MetadataLoad) {
  // Exercises creation, population, binding, and getting all data.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace), kTestProcessIdOrigin1);

  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, MetadataLoadWithMapOperations) {
  // Exercises creation, population, binding, and a map operation, and then
  // getting all the data.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace), kTestProcessIdOrigin1);

  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));

  EXPECT_CALL(listener_, OnCommitResult(DatabaseError::OK)).Times(1);
  test::PutSync(leveldb_1.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), base::nullopt, "");

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, CloneBeforeBind) {
  // Exercises cloning the namespace before we bind to the new cloned namespace.
  SessionStorageNamespaceImplMojo* namespace_impl1 =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);
  SessionStorageNamespaceImplMojo* namespace_impl2 =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id2_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl1->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  namespace_impl1->Bind(mojo::MakeRequest(&ss_namespace1),
                        kTestProcessIdOrigin1);
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();

  ASSERT_TRUE(namespace_impl2->IsPopulated());

  blink::mojom::SessionStorageNamespacePtr ss_namespace2;
  namespace_impl2->Bind(mojo::MakeRequest(&ss_namespace2),
                        kTestProcessIdOrigin1);
  blink::mojom::StorageAreaAssociatedPtr leveldb_2;
  ss_namespace2->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_2));

  // Do a put in the cloned namespace.
  EXPECT_CALL(listener_, OnCommitResult(DatabaseError::OK)).Times(2);
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  test::PutSync(leveldb_2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), base::nullopt, "");

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_2.get(), &data));
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, CloneAfterBind) {
  // Exercises cloning the namespace before we bind to the new cloned namespace.
  // Unlike the test above, we create a new area for the test_origin2_ in the
  // new namespace.
  SessionStorageNamespaceImplMojo* namespace_impl1 =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);
  SessionStorageNamespaceImplMojo* namespace_impl2 =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id2_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl1->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  namespace_impl1->Bind(mojo::MakeRequest(&ss_namespace1),
                        kTestProcessIdOrigin1);

  // Set that we are waiting for clone, so binding is possible.
  namespace_impl2->SetWaitingForClonePopulation();

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  // Get a new area.
  blink::mojom::SessionStorageNamespacePtr ss_namespace2;
  namespace_impl2->Bind(mojo::MakeRequest(&ss_namespace2),
                        kTestProcessIdAllOrigins);
  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o2;
  ss_namespace2->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_n2_o1));
  ss_namespace2->OpenArea(test_origin2_, mojo::MakeRequest(&leveldb_n2_o2));

  // Finally do the clone.
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();
  EXPECT_FALSE(bad_message_called_);
  ASSERT_TRUE(namespace_impl2->IsPopulated());

  // Do a put in the cloned namespace.
  EXPECT_CALL(listener_, OnCommitResult(DatabaseError::OK)).Times(1);
  test::PutSync(leveldb_n2_o2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), base::nullopt, "");

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  data.clear();
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o2.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key2"),
                                        StdStringToUint8Vector("data2"))));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, RemoveOriginData) {
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace), kTestProcessIdOrigin1);

  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));
  ss_namespace.FlushForTesting();

  // Create an observer to make sure the deletion is observed.
  testing::StrictMock<test::MockLevelDBObserver> mock_observer;
  mojo::AssociatedBinding<blink::mojom::StorageAreaObserver> observer_binding(
      &mock_observer);
  blink::mojom::StorageAreaObserverAssociatedPtrInfo observer_ptr_info;
  observer_binding.Bind(mojo::MakeRequest(&observer_ptr_info));
  leveldb_1->AddObserver(std::move(observer_ptr_info));
  leveldb_1.FlushForTesting();

  base::RunLoop loop;
  EXPECT_CALL(mock_observer, AllDeleted("\n"))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  EXPECT_CALL(listener_, OnCommitResult(DatabaseError::OK)).Times(1);
  namespace_impl->RemoveOriginData(test_origin1_);

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Check that the observer was notified.
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, RemoveOriginDataWithoutBinding) {
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  base::RunLoop loop;
  EXPECT_CALL(listener_, OnCommitResult(DatabaseError::OK))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  namespace_impl->RemoveOriginData(test_origin1_);
  loop.Run();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, ProcessLockedToOtherOrigin) {
  // Tries to open an area with a process that is locked to a different origin
  // and verifies the bad message callback.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace), kTestProcessIdOrigin1);
  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace->OpenArea(test_origin3_, mojo::MakeRequest(&leveldb_1));
  ss_namespace.FlushForTesting();
  EXPECT_TRUE(bad_message_called_);

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, PurgeUnused) {
  // Verifies that areas are kept alive after the area is unbound, and they
  // are removed when PurgeUnboundWrappers() is called.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace), kTestProcessIdOrigin1);

  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));
  EXPECT_TRUE(namespace_impl->HasAreaForOrigin(test_origin1_));

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  leveldb_1.reset();
  EXPECT_TRUE(namespace_impl->HasAreaForOrigin(test_origin1_));

  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(namespace_impl->HasAreaForOrigin(test_origin1_));

  namespaces_.clear();
}

TEST_F(SessionStorageNamespaceImplMojoTest, NamespaceBindingPerOrigin) {
  // Tries to open an area with a process that is locked to a different origin
  // and verifies the bad message callback.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace_o1;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace_o1),
                       kTestProcessIdOrigin1);
  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace_o1->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));
  ss_namespace_o1.FlushForTesting();
  EXPECT_FALSE(bad_message_called_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  blink::mojom::SessionStorageNamespacePtr ss_namespace_o2;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace_o2),
                       kTestProcessIdOrigin3);
  blink::mojom::StorageAreaAssociatedPtr leveldb_2;
  ss_namespace_o2->OpenArea(test_origin3_, mojo::MakeRequest(&leveldb_2));
  ss_namespace_o2.FlushForTesting();
  EXPECT_FALSE(bad_message_called_);

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  namespaces_.clear();
}
}  // namespace

TEST_F(SessionStorageNamespaceImplMojoTest, ReopenClonedAreaAfterPurge) {
  // Verifies that areas are kept alive after the area is unbound, and they
  // are removed when PurgeUnboundWrappers() is called.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  SessionStorageDataMap* data_map;
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .WillOnce(testing::SaveArg<1>(&data_map));

  namespace_impl->PopulateFromMetadata(
      &database_, metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  namespace_impl->Bind(mojo::MakeRequest(&ss_namespace), kTestProcessIdOrigin1);

  blink::mojom::StorageAreaAssociatedPtr leveldb_1;
  ss_namespace->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));

  // Save the data map, as if we did a clone:
  data_maps_[data_map->map_data()->MapNumberAsBytes()] = data_map;

  leveldb_1.reset();
  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(namespace_impl->HasAreaForOrigin(test_origin1_));

  ss_namespace->OpenArea(test_origin1_, mojo::MakeRequest(&leveldb_1));
  ss_namespace.FlushForTesting();

  EXPECT_EQ(namespace_impl->origin_areas_[test_origin1_]->data_map(), data_map);

  data_maps_.clear();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);

  namespaces_.clear();
}
}  // namespace content
