// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_namespace_impl_mojo.h"

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/session_storage_data_map.h"
#include "content/browser/dom_storage/session_storage_metadata.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/gmock_util.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

using NamespaceEntry = SessionStorageMetadata::NamespaceEntry;

constexpr const int kTestProcessIdOrigin1 = 11;
constexpr const int kTestProcessIdAllOrigins = 12;
constexpr const int kTestProcessIdOrigin3 = 13;

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

MATCHER(OKStatus, "Equality matcher for type OK leveldb::Status") {
  return arg.ok();
}

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() {}
  ~MockListener() override {}
  MOCK_METHOD2(OnDataMapCreation,
               void(const std::vector<uint8_t>& map_id,
                    SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(const std::vector<uint8_t>& map_id));
  MOCK_METHOD1(OnCommitResult, void(leveldb::Status));
};

class SessionStorageNamespaceImplMojoTest
    : public testing::Test,
      public SessionStorageNamespaceImplMojo::Delegate {
 public:
  SessionStorageNamespaceImplMojoTest()
      : test_namespace_id1_(base::GenerateGUID()),
        test_namespace_id2_(base::GenerateGUID()),
        test_origin1_(url::Origin::Create(GURL("https://host1.com/"))),
        test_origin2_(url::Origin::Create(GURL("https://host2.com/"))),
        test_origin3_(url::Origin::Create(GURL("https://host3.com/"))) {}
  ~SessionStorageNamespaceImplMojoTest() override = default;

  void RunBatch(
      std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> tasks) {
    base::RunLoop loop(base::RunLoop::Type::kNestableTasksAllowed);
    database_->RunBatchDatabaseTasks(
        std::move(tasks),
        base::BindLambdaForTesting([&](leveldb::Status) { loop.Quit(); }));
    loop.Run();
  }

  void SetUp() override {
    // Create a database that already has a namespace saved.
    base::RunLoop loop;
    database_ = storage::AsyncDomStorageDatabase::OpenInMemory(
        base::nullopt, "SessionStorageNamespaceImplMojoTest",
        base::CreateSequencedTaskRunner({base::MayBlock(), base::ThreadPool()}),
        base::BindLambdaForTesting([&](leveldb::Status) { loop.Quit(); }));
    loop.Run();

    metadata_.SetupNewDatabase();
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto entry = metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_);
    auto map_id = metadata_.RegisterNewMap(entry, test_origin1_, &save_tasks);
    DCHECK(map_id->KeyPrefix() == StdStringToUint8Vector("map-0-"));
    RunBatch(std::move(save_tasks));

    // Put some data in one of the maps.
    base::RunLoop put_loop;
    database_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          ASSERT_TRUE(db.Put(StdStringToUint8Vector("map-0-key1"),
                             StdStringToUint8Vector("data1"))
                          .ok());
          put_loop.Quit();
        }));
    put_loop.Run();

    auto* security_policy = ChildProcessSecurityPolicyImpl::GetInstance();
    security_policy->Add(kTestProcessIdOrigin1, &browser_context_);
    security_policy->Add(kTestProcessIdAllOrigins, &browser_context_);
    security_policy->Add(kTestProcessIdOrigin3, &browser_context_);
    security_policy->AddIsolatedOrigins(
        {test_origin1_, test_origin2_, test_origin3_},
        ChildProcessSecurityPolicy::IsolatedOriginSource::TEST);
    security_policy->LockToOrigin(IsolationContext(&browser_context_),
                                  kTestProcessIdOrigin1,
                                  test_origin1_.GetURL());
    security_policy->LockToOrigin(IsolationContext(&browser_context_),
                                  kTestProcessIdOrigin3,
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
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto map_data =
        metadata_.RegisterNewMap(namespace_entry, origin, &save_tasks);
    RunBatch(std::move(save_tasks));
    return map_data;
  }

  void RegisterShallowClonedNamespace(
      NamespaceEntry source_namespace,
      const std::string& destination_namespace,
      const SessionStorageNamespaceImplMojo::OriginAreas& areas_to_clone)
      override {
    std::vector<storage::AsyncDomStorageDatabase::BatchDatabaseTask> save_tasks;
    auto namespace_entry =
        metadata_.GetOrCreateNamespaceEntry(destination_namespace);
    metadata_.RegisterShallowClonedNamespace(source_namespace, namespace_entry,
                                             &save_tasks);
    RunBatch(std::move(save_tasks));

    auto it = namespaces_.find(destination_namespace);
    if (it == namespaces_.end()) {
      auto* namespace_impl =
          CreateSessionStorageNamespaceImplMojo(destination_namespace);
      namespace_impl->PopulateAsClone(database_.get(), namespace_entry,
                                      areas_to_clone);
      return;
    }
    it->second->PopulateAsClone(database_.get(), namespace_entry,
                                areas_to_clone);
  }

  scoped_refptr<SessionStorageDataMap> MaybeGetExistingDataMapForId(
      const std::vector<uint8_t>& map_number_as_bytes) override {
    auto it = data_maps_.find(map_number_as_bytes);
    if (it == data_maps_.end())
      return nullptr;
    return it->second;
  }

 protected:
  BrowserTaskEnvironment task_environment_;
  TestBrowserContext browser_context_;
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
  std::unique_ptr<storage::AsyncDomStorageDatabase> database_;
};

TEST_F(SessionStorageNamespaceImplMojoTest, MetadataLoad) {
  // Exercises creation, population, binding, and getting all data.
  SessionStorageNamespaceImplMojo* namespace_impl =
      CreateSessionStorageNamespaceImplMojo(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("0"), testing::_))
      .Times(1);

  namespace_impl->PopulateFromMetadata(
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  namespace_impl->Bind(ss_namespace.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);

  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace->OpenArea(test_origin1_,
                         leveldb_1.BindNewEndpointAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  namespace_impl->Bind(ss_namespace.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);

  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace->OpenArea(test_origin1_,
                         leveldb_1.BindNewEndpointAndPassReceiver());

  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce(testing::Invoke([&](auto error) { commit_loop.Quit(); }));
  test::PutSync(leveldb_1.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), base::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_1.get(), &data));
  EXPECT_EQ(2ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));
  EXPECT_TRUE(base::Contains(
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  namespace_impl1->Bind(ss_namespace1.BindNewPipeAndPassReceiver(),
                        kTestProcessIdOrigin1);
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();

  ASSERT_TRUE(namespace_impl2->IsPopulated());

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  namespace_impl2->Bind(ss_namespace2.BindNewPipeAndPassReceiver(),
                        kTestProcessIdOrigin1);
  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_2;
  ss_namespace2->OpenArea(test_origin1_,
                          leveldb_2.BindNewEndpointAndPassReceiver());

  // Do a put in the cloned namespace.
  base::RunLoop commit_loop;
  auto commit_callback = base::BarrierClosure(2, commit_loop.QuitClosure());
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(2)
      .WillRepeatedly(
          testing::Invoke([&](auto error) { commit_callback.Run(); }));
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  test::PutSync(leveldb_2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), base::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_2.get(), &data));
  EXPECT_EQ(2ul, data.size());
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  namespace_impl1->Bind(ss_namespace1.BindNewPipeAndPassReceiver(),
                        kTestProcessIdOrigin1);

  // Set that we are waiting for clone, so binding is possible.
  namespace_impl2->SetPendingPopulationFromParentNamespace(test_namespace_id1_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);
  // Get a new area.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  namespace_impl2->Bind(ss_namespace2.BindNewPipeAndPassReceiver(),
                        kTestProcessIdAllOrigins);
  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_n2_o1;
  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_n2_o2;
  ss_namespace2->OpenArea(test_origin1_,
                          leveldb_n2_o1.BindNewEndpointAndPassReceiver());
  ss_namespace2->OpenArea(test_origin2_,
                          leveldb_n2_o2.BindNewEndpointAndPassReceiver());

  // Finally do the clone.
  ss_namespace1->Clone(test_namespace_id2_);
  ss_namespace1.FlushForTesting();
  EXPECT_FALSE(bad_message_called_);
  ASSERT_TRUE(namespace_impl2->IsPopulated());

  // Do a put in the cloned namespace.
  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce(testing::Invoke([&](auto error) { commit_loop.Quit(); }));
  test::PutSync(leveldb_n2_o2.get(), StdStringToUint8Vector("key2"),
                StdStringToUint8Vector("data2"), base::nullopt, "");
  commit_loop.Run();

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
      data, blink::mojom::KeyValue::New(StdStringToUint8Vector("key1"),
                                        StdStringToUint8Vector("data1"))));

  data.clear();
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o2.get(), &data));
  EXPECT_EQ(1ul, data.size());
  EXPECT_TRUE(base::Contains(
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  namespace_impl->Bind(ss_namespace.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);

  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace->OpenArea(test_origin1_,
                         leveldb_1.BindNewEndpointAndPassReceiver());
  ss_namespace.FlushForTesting();

  // Create an observer to make sure the deletion is observed.
  testing::StrictMock<test::MockLevelDBObserver> mock_observer;
  mojo::AssociatedReceiver<blink::mojom::StorageAreaObserver> observer_receiver(
      &mock_observer);
  leveldb_1->AddObserver(observer_receiver.BindNewEndpointAndPassRemote());
  leveldb_1.FlushForTesting();

  base::RunLoop loop;
  EXPECT_CALL(mock_observer, AllDeleted("\n"))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));

  base::RunLoop commit_loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .Times(1)
      .WillOnce(testing::Invoke([&](auto error) { commit_loop.Quit(); }));
  namespace_impl->RemoveOriginData(test_origin1_, base::DoNothing());
  commit_loop.Run();

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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  base::RunLoop loop;
  EXPECT_CALL(listener_, OnCommitResult(OKStatus()))
      .WillOnce(base::test::RunClosure(loop.QuitClosure()));
  namespace_impl->RemoveOriginData(test_origin1_, base::DoNothing());
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  namespace_impl->Bind(ss_namespace.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);
  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace->OpenArea(test_origin3_,
                         leveldb_1.BindNewEndpointAndPassReceiver());
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  namespace_impl->Bind(ss_namespace.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);

  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace->OpenArea(test_origin1_,
                         leveldb_1.BindNewEndpointAndPassReceiver());
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace_o1;
  namespace_impl->Bind(ss_namespace_o1.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);
  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace_o1->OpenArea(test_origin1_,
                            leveldb_1.BindNewEndpointAndPassReceiver());
  ss_namespace_o1.FlushForTesting();
  EXPECT_FALSE(bad_message_called_);

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace_o2;
  namespace_impl->Bind(ss_namespace_o2.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin3);
  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_2;
  ss_namespace_o2->OpenArea(test_origin3_,
                            leveldb_2.BindNewEndpointAndPassReceiver());
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
      database_.get(),
      metadata_.GetOrCreateNamespaceEntry(test_namespace_id1_));

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  namespace_impl->Bind(ss_namespace.BindNewPipeAndPassReceiver(),
                       kTestProcessIdOrigin1);

  mojo::AssociatedRemote<blink::mojom::StorageArea> leveldb_1;
  ss_namespace->OpenArea(test_origin1_,
                         leveldb_1.BindNewEndpointAndPassReceiver());

  // Save the data map, as if we did a clone:
  data_maps_[data_map->map_data()->MapNumberAsBytes()] = data_map;

  leveldb_1.reset();
  namespace_impl->PurgeUnboundAreas();
  EXPECT_FALSE(namespace_impl->HasAreaForOrigin(test_origin1_));

  ss_namespace->OpenArea(test_origin1_,
                         leveldb_1.BindNewEndpointAndPassReceiver());
  ss_namespace.FlushForTesting();

  EXPECT_EQ(namespace_impl->origin_areas_[test_origin1_]->data_map(), data_map);

  data_maps_.clear();

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("0")))
      .Times(1);

  namespaces_.clear();
}
}  // namespace content
