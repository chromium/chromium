// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_context_mojo.h"

#include <stdint.h>
#include <vector>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "components/services/leveldb/public/cpp/util.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/browser/dom_storage/test/fake_leveldb_database_error_on_write.h"
#include "content/browser/dom_storage/test/fake_leveldb_service.h"
#include "content/browser/dom_storage/test/mojo_test_with_file_service.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/common/dom_storage/dom_storage_types.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/strong_associated_binding.h"
#include "services/file/file_service.h"
#include "services/file/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/service_context.h"
#include "services/service_manager/public/cpp/test/test_service_decorator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {
using leveldb::StdStringToUint8Vector;
using leveldb::String16ToUint8Vector;
using leveldb::Uint8VectorToStdString;
using leveldb::mojom::DatabaseError;
using leveldb::mojom::KeyValuePtr;

static const char kSessionStorageDirectory[] = "Session Storage";
static const int kTestProcessId = 0;

void GetStorageUsageCallback(base::OnceClosure callback,
                             std::vector<SessionStorageUsageInfo>* out_result,
                             std::vector<SessionStorageUsageInfo> result) {
  *out_result = std::move(result);
  std::move(callback).Run();
}

class SessionStorageContextMojoTest : public test::MojoTestWithFileService {
 public:
  SessionStorageContextMojoTest() {}

  ~SessionStorageContextMojoTest() override {
    if (context_)
      ShutdownContext();
  }

  void SetUp() override {
    features_.InitAndEnableFeature(blink::features::kOnionSoupDOMStorage);
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &SessionStorageContextMojoTest::OnBadMessage, base::Unretained(this)));
  }

  void TearDown() override {
    mojo::core::SetDefaultProcessErrorCallback(
        mojo::core::ProcessErrorCallback());
  }

  mojo::ReportBadMessageCallback GetBadMessageCallback() {
    return base::BindOnce(&SessionStorageContextMojoTest::OnBadMessage,
                          base::Unretained(this));
  }

  void OnBadMessage(const std::string& reason) { bad_message_called_ = true; }

  void SetBackingMode(SessionStorageContextMojo::BackingMode backing_mode) {
    DCHECK(!context_);
    backing_mode_ = backing_mode;
  }

  SessionStorageContextMojo* context() {
    if (!context_) {
      context_ = new SessionStorageContextMojo(
          base::SequencedTaskRunnerHandle::Get(), connector(), backing_mode_,
          base::FilePath(), kSessionStorageDirectory);
    }
    return context_;
  }

  void ShutdownContext() {
    context_->ShutdownAndDelete();
    context_ = nullptr;
    base::RunLoop().RunUntilIdle();
  }

  std::vector<SessionStorageUsageInfo> GetStorageUsageSync() {
    base::RunLoop run_loop;
    std::vector<SessionStorageUsageInfo> result;
    context()->GetStorageUsage(base::BindOnce(&GetStorageUsageCallback,
                                              run_loop.QuitClosure(), &result));
    run_loop.Run();
    return result;
  }

  void DoTestPut(const std::string& namespace_id,
                 const url::Origin& origin,
                 base::StringPiece key,
                 base::StringPiece value,
                 const std::string& source) {
    context()->CreateSessionNamespace(namespace_id);
    blink::mojom::SessionStorageNamespacePtr ss_namespace;
    context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace));
    blink::mojom::StorageAreaAssociatedPtr leveldb;
    ss_namespace->OpenArea(origin, mojo::MakeRequest(&leveldb));
    EXPECT_TRUE(test::PutSync(
        leveldb.get(), leveldb::StringPieceToUint8Vector(key),
        leveldb::StringPieceToUint8Vector(value), base::nullopt, source));
    context()->DeleteSessionNamespace(namespace_id, true);
  }

  base::Optional<std::vector<uint8_t>> DoTestGet(
      const std::string& namespace_id,
      const url::Origin& origin,
      base::StringPiece key) {
    context()->CreateSessionNamespace(namespace_id);
    blink::mojom::SessionStorageNamespacePtr ss_namespace;
    context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace));
    blink::mojom::StorageAreaAssociatedPtr leveldb;
    ss_namespace->OpenArea(origin, mojo::MakeRequest(&leveldb));

    // Use the GetAll interface because Gets are being removed.
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(leveldb.get(), &data));
    context()->DeleteSessionNamespace(namespace_id, true);

    std::vector<uint8_t> key_as_bytes = leveldb::StringPieceToUint8Vector(key);
    for (const auto& key_value : data) {
      if (key_value->key == key_as_bytes) {
        return key_value->value;
      }
    }
    return base::nullopt;
  }

 protected:
  bool bad_message_called_ = false;

 private:
  SessionStorageContextMojo::BackingMode backing_mode_ =
      SessionStorageContextMojo::BackingMode::kRestoreDiskState;
  base::test::ScopedFeatureList features_;
  SessionStorageContextMojo* context_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SessionStorageContextMojoTest);
};

TEST_F(SessionStorageContextMojoTest, MigrationV0ToV1) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://example.com"));
  base::string16 key = base::ASCIIToUTF16("key");
  base::string16 value = base::ASCIIToUTF16("value");
  base::string16 key2 = base::ASCIIToUTF16("key2");
  key2.push_back(0xd83d);
  key2.push_back(0xde00);

  base::FilePath old_db_path =
      temp_path().AppendASCII(kSessionStorageDirectory);
  {
    scoped_refptr<SessionStorageDatabase> db =
        base::MakeRefCounted<SessionStorageDatabase>(
            old_db_path, base::ThreadTaskRunnerHandle::Get().get());
    DOMStorageValuesMap data;
    data[key] = base::NullableString16(value, false);
    data[key2] = base::NullableString16(value, false);
    EXPECT_TRUE(db->CommitAreaChanges(namespace_id1, origin1, false, data));
    EXPECT_TRUE(db->CloneNamespace(namespace_id1, namespace_id2));
  }
  EXPECT_TRUE(base::PathExists(old_db_path));

  // The first call to context() here constructs it.
  context()->CreateSessionNamespace(namespace_id1);
  context()->CreateSessionNamespace(namespace_id2);

  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::SessionStorageNamespacePtr ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace2));

  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o2;
  ss_namespace2->OpenArea(origin1, mojo::MakeRequest(&leveldb_n2_o1));
  ss_namespace2->OpenArea(origin2, mojo::MakeRequest(&leveldb_n2_o2));

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  // There should have been a migration to get rid of the "map-0-" refcount
  // field.
  EXPECT_EQ(2ul, data.size());
  std::vector<uint8_t> key_as_vector =
      StdStringToUint8Vector(base::UTF16ToUTF8(key));
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(key_as_vector,
                                        String16ToUint8Vector(value))));
  EXPECT_TRUE(base::ContainsValue(
      data, blink::mojom::KeyValue::New(key_as_vector,
                                        String16ToUint8Vector(value))));
}

TEST_F(SessionStorageContextMojoTest, StartupShutdownSave) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));

  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  // Verify data is there.
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again.
  context()->DeleteSessionNamespace(namespace_id1, true);
  ShutdownContext();

  // This will re-open the context, and load the persisted namespace.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // The data from before should be here.
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Delete the namespace and shutdown the context and do not persist the data.
  context()->DeleteSessionNamespace(namespace_id1, false);
  ShutdownContext();

  // This will re-open the context, and the namespace should be empty.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // The data from before should not be here.
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, CloneBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  ss_namespace1->Clone(namespace_id2);
  leveldb_n1_o1.FlushForTesting();

  // Do the browser-side clone afterwards.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  blink::mojom::SessionStorageNamespacePtr ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace2));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
  ss_namespace2->OpenArea(origin1, mojo::MakeRequest(&leveldb_n2_o1));

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, Cloning) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // Context-triggered clone before the put. The clone doesn't actually count
  // until a clone comes from the namespace.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  ss_namespace1->Clone(namespace_id2);
  leveldb_n1_o1.FlushForTesting();

  // Open the second namespace.
  blink::mojom::SessionStorageNamespacePtr ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace2));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
  ss_namespace2->OpenArea(origin1, mojo::MakeRequest(&leveldb_n2_o1));

  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again. This tests the case where our cloning works even
  // though the namespace is deleted (but persisted on disk).
  context()->DeleteSessionNamespace(namespace_id1, true);

  // The data from before should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Put some data in namespace 2.
  EXPECT_TRUE(test::PutSync(
      leveldb_n2_o1.get(), leveldb::StringPieceToUint8Vector("key2"),
      leveldb::StringPieceToUint8Vector("value2"), base::nullopt, "source1"));
  EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
  EXPECT_EQ(2ul, data.size());

  // Re-open namespace 1, check that we don't have the extra data.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // We should only have the first value.
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, ImmediateCloning) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // Immediate clone.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kImmediate);

  // Open the second namespace, ensure empty.
  {
    blink::mojom::SessionStorageNamespacePtr ss_namespace2;
    context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace2));
    blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
    ss_namespace2->OpenArea(origin1, mojo::MakeRequest(&leveldb_n2_o1));
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
    EXPECT_EQ(0ul, data.size());
  }

  // Delete that namespace, copy again after a put.
  context()->DeleteSessionNamespace(namespace_id2, false);

  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value2"), base::nullopt, "source1"));

  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kImmediate);

  // Open the second namespace, ensure populated
  {
    blink::mojom::SessionStorageNamespacePtr ss_namespace2;
    context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace2));
    blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
    ss_namespace2->OpenArea(origin1, mojo::MakeRequest(&leveldb_n2_o1));
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(leveldb_n2_o1.get(), &data));
    EXPECT_EQ(1ul, data.size());
  }

  context()->DeleteSessionNamespace(namespace_id2, false);

  // Verify that cloning from the namespace object will result in a bad message.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kImmediate);

  // This should cause a bad message.
  ss_namespace1->Clone(namespace_id2);
  leveldb_n1_o1.FlushForTesting();

  EXPECT_TRUE(bad_message_called_);
}

TEST_F(SessionStorageContextMojoTest, Scavenging) {
  // Create our namespace, destroy our context and leave that namespace on disk,
  // and verify that it is scavenged if we re-create the context without calling
  // CreateSessionNamespace.

  // Create, verify we have no data.
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  // This scavenge call should NOT delete the namespace, as we just created it.
  {
    base::RunLoop loop;
    // Cause the connection to start loading, so we start scavenging mid-load.
    context()->Flush();
    context()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }
  // Restart context.
  ShutdownContext();
  context()->CreateSessionNamespace(namespace_id1);

  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  // This scavenge call should NOT delete the namespace, as we never called
  // delete.
  context()->ScavengeUnusedNamespaces(base::OnceClosure());

  // Restart context.
  ShutdownContext();
  context()->CreateSessionNamespace(namespace_id1);

  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again.
  context()->DeleteSessionNamespace(namespace_id1, true);

  // This scavenge call should NOT delete the namespace, as we explicity
  // persisted the namespace.
  {
    base::RunLoop loop;
    context()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }

  ShutdownContext();

  // Re-open the context, load the persisted namespace, and verify we still have
  // data.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Shutting down the context without an explicit DeleteSessionNamespace should
  // leave the data on disk.
  ShutdownContext();

  // Re-open the context, and scavenge should now remove the namespace as there
  // has been no call to CreateSessionNamespace. Check the data is empty.
  {
    base::RunLoop loop;
    context()->ScavengeUnusedNamespaces(loop.QuitClosure());
    loop.Run();
  }
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, InvalidVersionOnDisk) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin = url::Origin::Create(GURL("http://foobar.com"));

  // Create context and add some data to it (and check it's there).
  DoTestPut(namespace_id, origin, "key", "value", "source");
  base::Optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(leveldb::StringPieceToUint8Vector("value"), opt_value.value());

  ShutdownContext();
  {
    // Mess up version number in database.
    leveldb_env::ChromiumEnv env;
    std::unique_ptr<leveldb::DB> db;
    leveldb_env::Options options;
    options.env = &env;
    base::FilePath db_path =
        temp_path().Append(FILE_PATH_LITERAL("Session Storage"));
    ASSERT_TRUE(leveldb_env::OpenDB(options, db_path.AsUTF8Unsafe(), &db).ok());
    ASSERT_TRUE(db->Put(leveldb::WriteOptions(), "version", "argh").ok());
  }

  opt_value = DoTestGet(namespace_id, origin, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, origin, "key", "value", "source");

  ShutdownContext();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(leveldb::StringPieceToUint8Vector("value"), opt_value.value());
  ShutdownContext();
}

TEST_F(SessionStorageContextMojoTest, CorruptionOnDisk) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin = url::Origin::Create(GURL("http://foobar.com"));

  // Create context and add some data to it (and check it's there).
  DoTestPut(namespace_id, origin, "key", "value", "source");
  base::Optional<std::vector<uint8_t>> opt_value =
      DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(leveldb::StringPieceToUint8Vector("value"), opt_value.value());

  ShutdownContext();
  // Also flush Task Scheduler tasks to make sure the leveldb is fully closed.
  content::RunAllTasksUntilIdle();

  // Delete manifest files to mess up opening DB.
  base::FilePath db_path =
      temp_path().Append(FILE_PATH_LITERAL("Session Storage"));
  base::FileEnumerator file_enum(db_path, true, base::FileEnumerator::FILES,
                                 FILE_PATH_LITERAL("MANIFEST*"));
  for (base::FilePath name = file_enum.Next(); !name.empty();
       name = file_enum.Next()) {
    base::DeleteFile(name, false);
  }
  opt_value = DoTestGet(namespace_id, origin, "key");
  EXPECT_FALSE(opt_value);

  // Write data again.
  DoTestPut(namespace_id, origin, "key", "value", "source");

  ShutdownContext();

  // Data should have been preserved now.
  opt_value = DoTestGet(namespace_id, origin, "key");
  ASSERT_TRUE(opt_value);
  EXPECT_EQ(leveldb::StringPieceToUint8Vector("value"), opt_value.value());
  ShutdownContext();
}

TEST_F(SessionStorageContextMojoTest, RecreateOnCommitFailure) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://asf.com"));
  url::Origin origin3 = url::Origin::Create(GURL("http://example.com"));

  test::FakeLevelDBService fake_leveldb_service;
  ResetFileServiceAndConnector(
      service_manager::TestServiceDecorator::CreateServiceWithUniqueOverride(
          file::CreateFileService(), leveldb::mojom::LevelDBService::Name_,
          base::BindRepeating(&test::FakeLevelDBService::Bind,
                              base::Unretained(&fake_leveldb_service))));

  // Open three connections to the database.
  blink::mojom::StorageAreaAssociatedPtr area1;
  blink::mojom::StorageAreaAssociatedPtr area2;
  blink::mojom::StorageAreaAssociatedPtr area3;
  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  context()->CreateSessionNamespace(namespace_id);
  {
    base::RunLoop loop;
    fake_leveldb_service.SetOnOpenCallback(loop.QuitClosure());
    context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace));
    ss_namespace->OpenArea(origin1, mojo::MakeRequest(&area1));
    ss_namespace->OpenArea(origin2, mojo::MakeRequest(&area2));
    ss_namespace->OpenArea(origin3, mojo::MakeRequest(&area3));
    loop.Run();
  }

  // Verify one attempt was made to open the database, and connect that request
  // with a database implementation that always fails on write.
  ASSERT_EQ(1u, fake_leveldb_service.open_requests().size());
  auto& open_request = fake_leveldb_service.open_requests()[0];
  std::map<std::vector<uint8_t>, std::vector<uint8_t>> test_data;
  auto mock_db = mojo::MakeStrongAssociatedBinding(
      std::make_unique<test::FakeLevelDBDatabaseErrorOnWrite>(&test_data),
      std::move(open_request.request));
  std::move(open_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  fake_leveldb_service.open_requests().clear();

  // Setup a RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  base::RunLoop reopen_loop;
  fake_leveldb_service.SetOnOpenCallback(reopen_loop.QuitClosure());

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first origin. This put operation should result in a
  // pending commit that will get cancelled when the database connection is
  // closed.
  auto value = leveldb::StringPieceToUint8Vector("avalue");
  area3->Put(leveldb::StringPieceToUint8Vector("w3key"), value, base::nullopt,
             "source",
             base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  int i = 0;
  while (!area1.encountered_error()) {
    ++i;
    base::RunLoop put_loop;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    std::vector<uint8_t> old_value = value;
    value[0]++;
    area1.set_connection_error_handler(put_loop.QuitClosure());

    area1->Put(leveldb::StringPieceToUint8Vector("key"), value, base::nullopt,
               "source", base::BindLambdaForTesting([&](bool success) {
                 EXPECT_TRUE(success);
               }));
    area1.FlushForTesting();
    put_loop.RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushAreaForTesting(namespace_id, origin1);
  }
  // Make sure all messages to the DB have been processed (Flush above merely
  // schedules a commit, but there is no guarantee about those having been
  // processed yet).
  fake_leveldb_service.FlushBindingsForTesting();
  if (mock_db)
    mock_db->FlushForTesting();
  // At this point enough commit failures should have happened to cause the
  // connection to the database to have been severed.
  EXPECT_FALSE(mock_db);

  // The connection to the second area should have closed as well.
  EXPECT_TRUE(area2.encountered_error());
  EXPECT_TRUE(ss_namespace.encountered_error());

  // And the old database should have been destroyed.
  EXPECT_EQ(1u, fake_leveldb_service.destroy_requests().size());

  // Reconnect area1 to the database, and try to read a value.
  context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace));
  ss_namespace->OpenArea(origin1, mojo::MakeRequest(&area1));

  base::RunLoop delete_loop;
  bool success = true;
  test::MockLevelDBObserver observer4;
  area1->AddObserver(observer4.Bind());
  area1->Delete(leveldb::StringPieceToUint8Vector("key"), base::nullopt,
                "source", base::BindLambdaForTesting([&](bool success_in) {
                  success = success_in;
                  delete_loop.Quit();
                }));

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // connect that new request to a properly functioning database.
  reopen_loop.Run();
  ASSERT_EQ(1u, fake_leveldb_service.open_requests().size());
  auto& reopen_request = fake_leveldb_service.open_requests()[0];
  mock_db = mojo::MakeStrongAssociatedBinding(
      std::make_unique<FakeLevelDBDatabase>(&test_data),
      std::move(reopen_request.request));
  std::move(reopen_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  fake_leveldb_service.open_requests().clear();

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  area1 = nullptr;
  ss_namespace.reset();
  context()->DeleteSessionNamespace(namespace_id, true);

  {
    // Committing data should now work.
    DoTestPut(namespace_id, origin1, "key", "value", "source");
    base::Optional<std::vector<uint8_t>> opt_value =
        DoTestGet(namespace_id, origin1, "key");
    ASSERT_TRUE(opt_value);
    EXPECT_EQ(leveldb::StringPieceToUint8Vector("value"), opt_value.value());
  }
}

TEST_F(SessionStorageContextMojoTest, DontRecreateOnRepeatedCommitFailure) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  test::FakeLevelDBService fake_leveldb_service;
  ResetFileServiceAndConnector(
      service_manager::TestServiceDecorator::CreateServiceWithUniqueOverride(
          file::CreateFileService(), leveldb::mojom::LevelDBService::Name_,
          base::BindRepeating(&test::FakeLevelDBService::Bind,
                              base::Unretained(&fake_leveldb_service))));

  std::map<std::vector<uint8_t>, std::vector<uint8_t>> test_data;

  // Open three connections to the database.
  blink::mojom::StorageAreaAssociatedPtr area;
  blink::mojom::SessionStorageNamespacePtr ss_namespace;
  context()->CreateSessionNamespace(namespace_id);
  {
    base::RunLoop loop;
    fake_leveldb_service.SetOnOpenCallback(loop.QuitClosure());
    context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace));
    ss_namespace->OpenArea(origin1, mojo::MakeRequest(&area));
    loop.Run();
  }

  // Verify one attempt was made to open the database, and connect that request
  // with a database implementation that always fails on write.
  ASSERT_EQ(1u, fake_leveldb_service.open_requests().size());
  auto& open_request = fake_leveldb_service.open_requests()[0];
  auto mock_db = mojo::MakeStrongAssociatedBinding(
      std::make_unique<test::FakeLevelDBDatabaseErrorOnWrite>(&test_data),
      std::move(open_request.request));
  std::move(open_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  fake_leveldb_service.open_requests().clear();

  // Setup a RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  base::RunLoop reopen_loop;
  fake_leveldb_service.SetOnOpenCallback(reopen_loop.QuitClosure());

  // Repeatedly write data to the database, to trigger enough commit errors.
  auto value = leveldb::StringPieceToUint8Vector("avalue");
  base::Optional<std::vector<uint8_t>> old_value = base::nullopt;
  while (!area.encountered_error()) {
    base::RunLoop put_loop;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area.set_connection_error_handler(put_loop.QuitClosure());
    area->Put(leveldb::StringPieceToUint8Vector("key"), value, old_value,
              "source", base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                put_loop.Quit();
              }));
    area.FlushForTesting();
    put_loop.RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushAreaForTesting(namespace_id, origin1);

    old_value = value;
    value[0]++;
  }
  // Make sure all messages to the DB have been processed (Flush above merely
  // schedules a commit, but there is no guarantee about those having been
  // processed yet).
  if (mock_db)
    mock_db->FlushForTesting();
  // At this point enough commit failures should have happened to cause the
  // connection to the database to have been severed.
  EXPECT_FALSE(mock_db);

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  reopen_loop.Run();
  ASSERT_EQ(1u, fake_leveldb_service.open_requests().size());
  auto& reopen_request = fake_leveldb_service.open_requests()[0];
  mock_db = mojo::MakeStrongAssociatedBinding(
      std::make_unique<test::FakeLevelDBDatabaseErrorOnWrite>(&test_data),
      std::move(reopen_request.request));
  std::move(reopen_request.callback).Run(leveldb::mojom::DatabaseError::OK);
  fake_leveldb_service.open_requests().clear();

  // The old database should also have been destroyed.
  EXPECT_EQ(1u, fake_leveldb_service.destroy_requests().size());

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace));
  ss_namespace->OpenArea(origin1, mojo::MakeRequest(&area));
  old_value = base::nullopt;
  for (int i = 0; i < 64; ++i) {
    base::RunLoop put_loop;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area.set_connection_error_handler(put_loop.QuitClosure());
    area->Put(leveldb::StringPieceToUint8Vector("key"), value, old_value,
              "source", base::BindLambdaForTesting([&](bool success) {
                EXPECT_TRUE(success);
                put_loop.Quit();
              }));
    area.FlushForTesting();
    put_loop.RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushAreaForTesting(namespace_id, origin1);

    old_value = value;
    value[0]++;
  }
  // Make sure all messages to the DB have been processed (Flush above merely
  // schedules a commit, but there is no guarantee about those having been
  // processed yet).
  if (mock_db)
    mock_db->FlushForTesting();
  EXPECT_TRUE(mock_db);
  EXPECT_FALSE(area.encountered_error());

  context()->DeleteSessionNamespace(namespace_id, false);
}

TEST_F(SessionStorageContextMojoTest, GetUsage) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));
  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  base::RunLoop loop;
  context()->GetStorageUsage(base::BindLambdaForTesting(
      [&](std::vector<SessionStorageUsageInfo> usage) {
        loop.Quit();
        ASSERT_EQ(1u, usage.size());
        EXPECT_EQ(origin1.GetURL(), usage[0].origin);
        EXPECT_EQ(namespace_id1, usage[0].namespace_id);
      }));
  loop.Run();
}

TEST_F(SessionStorageContextMojoTest, DeleteStorage) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  // First, test deleting data for a namespace that is open.
  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  context()->DeleteStorage(origin1, namespace_id1);

  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Next, test that it deletes the data even if there isn't a namespace open.
  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));
  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again.
  context()->DeleteSessionNamespace(namespace_id1, true);
  ShutdownContext();

  // This restarts the context, then deletes the storage.
  context()->DeleteStorage(origin1, namespace_id1);

  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));
  data.clear();
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, PurgeInactiveWrappers) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb));

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(
      leveldb.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));
  context()->FlushAreaForTesting(namespace_id1, origin1);

  ss_namespace1.reset();
  leveldb.reset();

  // Clear all the data from the backing database.
  base::RunLoop loop;
  context()->DatabaseForTesting()->DeletePrefixed(
      leveldb::StringPieceToUint8Vector("map"),
      base::BindLambdaForTesting([&](DatabaseError status) {
        loop.Quit();
        EXPECT_EQ(DatabaseError::OK, status);
      }));
  loop.Run();

  // Now open many new wrappers (for different origins) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    blink::mojom::SessionStorageNamespacePtr ss_namespace1;
    context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                  GetBadMessageCallback(),
                                  mojo::MakeRequest(&ss_namespace1));
    blink::mojom::StorageAreaAssociatedPtr leveldb;
    ss_namespace1->OpenArea(url::Origin::Create(GURL(base::StringPrintf(
                                "http://example.com:%d", i))),
                            mojo::MakeRequest(&leveldb));
    base::RunLoop().RunUntilIdle();
    ss_namespace1.reset();
    leveldb.reset();
  }

  // And make sure caches were actually cleared.
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb));
  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(leveldb.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, ClearDiskState) {
  SetBackingMode(SessionStorageContextMojo::BackingMode::kClearDiskStateOnOpen);
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));

  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));

  // Delete the namespace and shutdown the context, BUT persist the namespace on
  // disk.
  context()->DeleteSessionNamespace(namespace_id1, true);
  ShutdownContext();

  // This will re-open the context, and load the persisted namespace, but it
  // should have been deleted due to our backing mode.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  // The data from before should not be here, because the context clears disk
  // space on open.
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

}  // namespace

TEST_F(SessionStorageContextMojoTest, PurgeMemoryDoesNotCrashOrHang) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  context()->CreateSessionNamespace(namespace_id1);
  blink::mojom::SessionStorageNamespacePtr ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace1));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n1_o1;
  ss_namespace1->OpenArea(origin1, mojo::MakeRequest(&leveldb_n1_o1));

  context()->CreateSessionNamespace(namespace_id2);
  blink::mojom::SessionStorageNamespacePtr ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                mojo::MakeRequest(&ss_namespace2));
  blink::mojom::StorageAreaAssociatedPtr leveldb_n2_o1;
  ss_namespace2->OpenArea(origin1, mojo::MakeRequest(&leveldb_n2_o1));

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(
      leveldb_n1_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value1"), base::nullopt, "source1"));
  EXPECT_TRUE(test::PutSync(
      leveldb_n2_o1.get(), leveldb::StringPieceToUint8Vector("key1"),
      leveldb::StringPieceToUint8Vector("value2"), base::nullopt, "source1"));

  context()->FlushAreaForTesting(namespace_id1, origin1);

  leveldb_n2_o1.reset();
  ss_namespace2.reset();

  base::RunLoop().RunUntilIdle();

  // Verify this doesn't crash or hang.
  context()->PurgeMemory();

  size_t memory_used = context()
                           ->namespaces_[namespace_id1]
                           ->origin_areas_[origin1]
                           ->data_map()
                           ->storage_area()
                           ->memory_used();
  EXPECT_EQ(0ul, memory_used);

  // Test the values is still there.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(leveldb_n1_o1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  base::Optional<std::vector<uint8_t>> opt_value2 =
      DoTestGet(namespace_id2, origin1, "key1");
  ASSERT_TRUE(opt_value2);
  EXPECT_EQ(leveldb::StringPieceToUint8Vector("value2"), opt_value2.value());
}

}  // namespace content
