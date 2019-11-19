// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_data_map.h"

#include <map>
#include <vector>

#include "base/bind.h"
#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "components/services/storage/dom_storage/async_dom_storage_database.h"
#include "components/services/storage/dom_storage/dom_storage_database.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
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

base::span<const uint8_t> MakeBytes(base::StringPiece str) {
  return base::as_bytes(base::make_span(str));
}

class MockListener : public SessionStorageDataMap::Listener {
 public:
  MockListener() {}
  ~MockListener() override {}
  MOCK_METHOD2(OnDataMapCreation,
               void(const std::vector<uint8_t>& map_id,
                    SessionStorageDataMap* map));
  MOCK_METHOD1(OnDataMapDestruction, void(const std::vector<uint8_t>& map_id));
  MOCK_METHOD1(OnCommitResult, void(leveldb::Status status));
};

void GetAllDataCallback(bool* success_out,
                        std::vector<blink::mojom::KeyValuePtr>* data_out,
                        bool success,
                        std::vector<blink::mojom::KeyValuePtr> data) {
  *success_out = success;
  *data_out = std::move(data);
}

base::OnceCallback<void(bool success,
                        std::vector<blink::mojom::KeyValuePtr> data)>
MakeGetAllCallback(bool* sucess_out,
                   std::vector<blink::mojom::KeyValuePtr>* data_out) {
  return base::BindOnce(&GetAllDataCallback, sucess_out, data_out);
}

class GetAllCallback : public blink::mojom::StorageAreaGetAllCallback {
 public:
  static mojo::PendingAssociatedRemote<blink::mojom::StorageAreaGetAllCallback>
  CreateAndBind(bool* result, base::OnceClosure callback) {
    mojo::AssociatedRemote<blink::mojom::StorageAreaGetAllCallback> remote;
    mojo::MakeSelfOwnedAssociatedReceiver(
        base::WrapUnique(new GetAllCallback(result, std::move(callback))),
        remote.BindNewEndpointAndPassDedicatedReceiverForTesting());
    return remote.Unbind();
  }

 private:
  GetAllCallback(bool* result, base::OnceClosure callback)
      : result_(result), callback_(std::move(callback)) {}
  void Complete(bool success) override {
    *result_ = success;
    if (callback_)
      std::move(callback_).Run();
  }

  bool* result_;
  base::OnceClosure callback_;
};

class SessionStorageDataMapTest : public testing::Test {
 public:
  SessionStorageDataMapTest()
      : test_origin_(url::Origin::Create(GURL("http://host1.com:1"))) {
    base::RunLoop loop;
    database_ = storage::AsyncDomStorageDatabase::OpenInMemory(
        base::nullopt, "SessionStorageDataMapTest",
        base::CreateSequencedTaskRunner({base::MayBlock(), base::ThreadPool()}),
        base::BindLambdaForTesting([&](leveldb::Status status) {
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    database_->database().PostTaskWithThisObject(
        FROM_HERE, base::BindOnce([](const storage::DomStorageDatabase& db) {
          // Should show up in first map.
          leveldb::Status status =
              db.Put(MakeBytes("map-1-key1"), MakeBytes("data1"));
          ASSERT_TRUE(status.ok());

          // Dummy data to verify we don't delete everything.
          status = db.Put(MakeBytes("map-3-key1"), MakeBytes("data3"));
          ASSERT_TRUE(status.ok());
        }));
  }

  ~SessionStorageDataMapTest() override = default;

  std::map<std::string, std::string> GetDatabaseContents() {
    std::vector<storage::DomStorageDatabase::KeyValuePair> entries;
    base::RunLoop loop;
    database_->database().PostTaskWithThisObject(
        FROM_HERE,
        base::BindLambdaForTesting([&](const storage::DomStorageDatabase& db) {
          leveldb::Status status = db.GetPrefixed({}, &entries);
          ASSERT_TRUE(status.ok());
          loop.Quit();
        }));
    loop.Run();

    std::map<std::string, std::string> contents;
    for (auto& entry : entries) {
      contents.emplace(std::string(entry.key.begin(), entry.key.end()),
                       std::string(entry.value.begin(), entry.value.end()));
    }

    return contents;
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  testing::StrictMock<MockListener> listener_;
  url::Origin test_origin_;
  std::unique_ptr<storage::AsyncDomStorageDatabase> database_;
};

}  // namespace

TEST_F(SessionStorageDataMapTest, BasicEmptyCreation) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  scoped_refptr<SessionStorageDataMap> map =
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          base::MakeRefCounted<SessionStorageMetadata::MapData>(1,
                                                                test_origin_),
          database_.get());

  bool success;
  std::vector<blink::mojom::KeyValuePtr> data;
  bool done = false;
  base::RunLoop loop;
  map->storage_area()->GetAll(
      GetAllCallback::CreateAndBind(&done, loop.QuitClosure()),
      MakeGetAllCallback(&success, &data));
  loop.Run();

  EXPECT_TRUE(done);
  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(StdStringToUint8Vector("key1"), data[0]->key);
  EXPECT_EQ(StdStringToUint8Vector("data1"), data[0]->value);

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  // Test data is not cleared on deletion.
  map = nullptr;
  EXPECT_EQ(2u, GetDatabaseContents().size());
}

TEST_F(SessionStorageDataMapTest, ExplicitlyEmpty) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  scoped_refptr<SessionStorageDataMap> map = SessionStorageDataMap::CreateEmpty(
      &listener_,
      base::MakeRefCounted<SessionStorageMetadata::MapData>(1, test_origin_),
      database_.get());

  bool success;
  std::vector<blink::mojom::KeyValuePtr> data;
  bool done = false;
  base::RunLoop loop;
  map->storage_area()->GetAll(
      GetAllCallback::CreateAndBind(&done, loop.QuitClosure()),
      MakeGetAllCallback(&success, &data));
  loop.Run();

  EXPECT_TRUE(done);
  ASSERT_EQ(0u, data.size());

  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);

  // Test data is not cleared on deletion.
  map = nullptr;
  EXPECT_EQ(2u, GetDatabaseContents().size());
}

TEST_F(SessionStorageDataMapTest, Clone) {
  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("1"), testing::_))
      .Times(1);

  scoped_refptr<SessionStorageDataMap> map1 =
      SessionStorageDataMap::CreateFromDisk(
          &listener_,
          base::MakeRefCounted<SessionStorageMetadata::MapData>(1,
                                                                test_origin_),
          database_.get());

  EXPECT_CALL(listener_,
              OnDataMapCreation(StdStringToUint8Vector("2"), testing::_))
      .Times(1);
  // One call on fork.
  EXPECT_CALL(listener_, OnCommitResult(OKStatus())).Times(1);

  scoped_refptr<SessionStorageDataMap> map2 =
      SessionStorageDataMap::CreateClone(
          &listener_,
          base::MakeRefCounted<SessionStorageMetadata::MapData>(2,
                                                                test_origin_),
          map1);

  bool success;
  std::vector<blink::mojom::KeyValuePtr> data;
  bool done = false;
  base::RunLoop loop;
  map2->storage_area()->GetAll(
      GetAllCallback::CreateAndBind(&done, loop.QuitClosure()),
      MakeGetAllCallback(&success, &data));
  loop.Run();

  EXPECT_TRUE(done);
  ASSERT_EQ(1u, data.size());
  EXPECT_EQ(StdStringToUint8Vector("key1"), data[0]->key);
  EXPECT_EQ(StdStringToUint8Vector("data1"), data[0]->value);

  // Test that the data was copied.
  EXPECT_EQ("data1", GetDatabaseContents()["map-2-key1"]);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("1")))
      .Times(1);
  EXPECT_CALL(listener_, OnDataMapDestruction(StdStringToUint8Vector("2")))
      .Times(1);

  // Test data is not cleared on deletion.
  map1 = nullptr;
  map2 = nullptr;
  EXPECT_EQ(3u, GetDatabaseContents().size());
}

}  // namespace content
