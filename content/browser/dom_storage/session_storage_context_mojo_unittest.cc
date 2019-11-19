// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/dom_storage/session_storage_context_mojo.h"

#include <stdint.h>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/guid.h"
#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/dom_storage/dom_storage_types.h"
#include "content/browser/dom_storage/session_storage_database.h"
#include "content/browser/dom_storage/test/storage_area_test_util.h"
#include "content/public/browser/session_storage_usage_info.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_utils.h"
#include "mojo/core/embedder/embedder.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

namespace content {
namespace {

std::vector<uint8_t> StdStringToUint8Vector(const std::string& s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> StringPieceToUint8Vector(base::StringPiece s) {
  return std::vector<uint8_t>(s.begin(), s.end());
}

std::vector<uint8_t> String16ToUint8Vector(const base::string16& s) {
  auto bytes = base::as_bytes(base::make_span(s));
  return std::vector<uint8_t>(bytes.begin(), bytes.end());
}

static const char kSessionStorageDirectory[] = "Session Storage";
static const int kTestProcessId = 0;

void GetStorageUsageCallback(base::OnceClosure callback,
                             std::vector<SessionStorageUsageInfo>* out_result,
                             std::vector<SessionStorageUsageInfo> result) {
  *out_result = std::move(result);
  std::move(callback).Run();
}

class SessionStorageContextMojoTest : public testing::Test {
 public:
  SessionStorageContextMojoTest() { CHECK(temp_dir_.CreateUniqueTempDir()); }

  ~SessionStorageContextMojoTest() override {
    if (context_)
      ShutdownContext();

    // There may be pending tasks to clean up files in the temp dir. Make sure
    // they run so temp dir deletion can succeed.
    RunUntilIdle();

    EXPECT_TRUE(temp_dir_.Delete());
  }

  void SetUp() override {
    mojo::core::SetDefaultProcessErrorCallback(base::BindRepeating(
        &SessionStorageContextMojoTest::OnBadMessage, base::Unretained(this)));

    ChildProcessSecurityPolicyImpl::GetInstance()->Add(kTestProcessId,
                                                       &browser_context_);
  }

  void TearDown() override {
    if (context_)
      ShutdownContext();
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(kTestProcessId);

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
          temp_path(), blocking_task_runner_,
          base::SequencedTaskRunnerHandle::Get(), backing_mode_,
          kSessionStorageDirectory);
    }
    return context_;
  }

  void ShutdownContext() {
    context_->ShutdownAndDelete();
    context_ = nullptr;
    RunUntilIdle();
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
    mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
    context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                  GetBadMessageCallback(),
                                  ss_namespace.BindNewPipeAndPassReceiver());
    mojo::AssociatedRemote<blink::mojom::StorageArea> area;
    ss_namespace->OpenArea(origin, area.BindNewEndpointAndPassReceiver());
    EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector(key),
                              StringPieceToUint8Vector(value), base::nullopt,
                              source));
    context()->DeleteSessionNamespace(namespace_id, true);
  }

  base::Optional<std::vector<uint8_t>> DoTestGet(
      const std::string& namespace_id,
      const url::Origin& origin,
      base::StringPiece key) {
    context()->CreateSessionNamespace(namespace_id);
    mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
    context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                  GetBadMessageCallback(),
                                  ss_namespace.BindNewPipeAndPassReceiver());
    mojo::AssociatedRemote<blink::mojom::StorageArea> area;
    ss_namespace->OpenArea(origin, area.BindNewEndpointAndPassReceiver());

    // Use the GetAll interface because Gets are being removed.
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area.get(), &data));
    context()->DeleteSessionNamespace(namespace_id, true);

    std::vector<uint8_t> key_as_bytes = StringPieceToUint8Vector(key);
    for (const auto& key_value : data) {
      if (key_value->key == key_as_bytes) {
        return key_value->value;
      }
    }
    return base::nullopt;
  }

 protected:
  const base::FilePath& temp_path() const { return temp_dir_.GetPath(); }
  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  bool bad_message_called_ = false;

 private:
  BrowserTaskEnvironment task_environment_;
  base::ScopedTempDir temp_dir_;
  TestBrowserContext browser_context_;
  SessionStorageContextMojo::BackingMode backing_mode_ =
      SessionStorageContextMojo::BackingMode::kRestoreDiskState;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_{
      base::CreateSequencedTaskRunner(
          {base::MayBlock(), base::ThreadPool(),
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})};
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

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());

  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2_o1;
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2_o2;
  ss_namespace2->OpenArea(origin1, area_n2_o1.BindNewEndpointAndPassReceiver());
  ss_namespace2->OpenArea(origin2, area_n2_o2.BindNewEndpointAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2_o1.get(), &data));
  // There should have been a migration to get rid of the "map-0-" refcount
  // field.
  EXPECT_EQ(2ul, data.size());
  std::vector<uint8_t> key_as_vector =
      StdStringToUint8Vector(base::UTF16ToUTF8(key));
  EXPECT_TRUE(
      base::Contains(data, blink::mojom::KeyValue::New(
                               key_as_vector, String16ToUint8Vector(value))));
  EXPECT_TRUE(
      base::Contains(data, blink::mojom::KeyValue::New(
                               key_as_vector, String16ToUint8Vector(value))));
}

TEST_F(SessionStorageContextMojoTest, StartupShutdownSave) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());

  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Verify data is there.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();
  ss_namespace1.reset();

  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again.
  context()->DeleteSessionNamespace(namespace_id1, true);
  ShutdownContext();

  // This will re-open the context, and load the persisted namespace.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // The data from before should be here.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();
  ss_namespace1.reset();

  // Delete the namespace and shutdown the context and do not persist the data.
  context()->DeleteSessionNamespace(namespace_id1, false);
  ShutdownContext();

  // This will re-open the context, and the namespace should be empty.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // The data from before should not be here.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, CloneBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();

  // Do the browser-side clone afterwards.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, Cloning) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Context-triggered clone before the put. The clone doesn't actually count
  // until a clone comes from the namespace.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();
  area_n1.reset();
  ss_namespace1.reset();

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again. This tests the case where our cloning works even
  // though the namespace is deleted (but persisted on disk).
  context()->DeleteSessionNamespace(namespace_id1, true);

  // The data from before should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());

  // Put some data in namespace 2.
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringPieceToUint8Vector("key2"),
                            StringPieceToUint8Vector("value2"), base::nullopt,
                            "source1"));
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(2ul, data.size());

  // Re-open namespace 1, check that we don't have the extra data.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // We should only have the first value.
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, ImmediateCloning) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Immediate clone.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kImmediate);

  // Open the second namespace, ensure empty.
  {
    mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
    context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                  GetBadMessageCallback(),
                                  ss_namespace2.BindNewPipeAndPassReceiver());
    mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
    ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
    EXPECT_EQ(0ul, data.size());
  }

  // Delete that namespace, copy again after a put.
  context()->DeleteSessionNamespace(namespace_id2, false);

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value2"), base::nullopt,
                            "source1"));

  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kImmediate);

  // Open the second namespace, ensure populated
  {
    mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
    context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                  GetBadMessageCallback(),
                                  ss_namespace2.BindNewPipeAndPassReceiver());
    mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
    ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());
    std::vector<blink::mojom::KeyValuePtr> data;
    EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
    EXPECT_EQ(1ul, data.size());
  }

  context()->DeleteSessionNamespace(namespace_id2, false);

  // Verify that cloning from the namespace object will result in a bad message.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kImmediate);

  // This should cause a bad message.
  ss_namespace1->Clone(namespace_id2);
  area_n1.FlushForTesting();

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

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  area_n1.reset();
  ss_namespace1.reset();

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
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());
  area_n1.reset();
  ss_namespace1.reset();

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
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
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
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());

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
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());
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
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());

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
  EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());
  ShutdownContext();
}

TEST_F(SessionStorageContextMojoTest, RecreateOnCommitFailure) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  url::Origin origin2 = url::Origin::Create(GURL("http://asf.com"));
  url::Origin origin3 = url::Origin::Create(GURL("http://example.com"));

  base::Optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));

  open_loop.emplace();

  // Open three connections to the database.
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_o1;
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_o2;
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_o3;
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  context()->CreateSessionNamespace(namespace_id);

  context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                GetBadMessageCallback(),
                                ss_namespace.BindNewPipeAndPassReceiver());
  ss_namespace->OpenArea(origin1, area_o1.BindNewEndpointAndPassReceiver());
  ss_namespace->OpenArea(origin2, area_o2.BindNewEndpointAndPassReceiver());
  ss_namespace->OpenArea(origin3, area_o3.BindNewEndpointAndPassReceiver());
  open_loop->Run();

  // Ensure that the first opened database always fails to write data.
  context()->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE,
      base::BindLambdaForTesting([&](storage::DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  ASSERT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();

  // Also prepare for another database connection, next time providing a
  // functioning database.
  open_loop.emplace();
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));

  // Start a put operation on the third connection before starting to commit
  // a lot of data on the first origin. This put operation should result in a
  // pending commit that will get cancelled when the database connection is
  // closed.
  auto value = StringPieceToUint8Vector("avalue");
  area_o3->Put(StringPieceToUint8Vector("w3key"), value, base::nullopt,
               "source",
               base::BindOnce([](bool success) { EXPECT_TRUE(success); }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  int i = 0;
  while (area_o1.is_connected()) {
    ++i;
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    std::vector<uint8_t> old_value = value;
    value[0]++;
    area_o1->Put(StringPieceToUint8Vector("key"), value, base::nullopt,
                 "source", base::BindLambdaForTesting([&](bool success) {
                   EXPECT_TRUE(success);
                 }));
    area_o1.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushAreaForTesting(namespace_id, origin1);
  }
  area_o1.reset();

  // Wait for a new database to be opened, which should happen after the first
  // database is destroyed due to failures.
  open_loop->Run();
  EXPECT_EQ(1u, num_databases_destroyed);
  EXPECT_EQ(2u, num_database_open_requests);

  // The connection to the second area should have closed as well.
  area_o2.FlushForTesting();
  ss_namespace.FlushForTesting();
  EXPECT_FALSE(area_o2.is_connected());
  EXPECT_FALSE(ss_namespace.is_connected());

  // Reconnect area_o1 to the new database, and try to read a value.
  ss_namespace.reset();
  context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                GetBadMessageCallback(),
                                ss_namespace.BindNewPipeAndPassReceiver());
  ss_namespace->OpenArea(origin1, area_o1.BindNewEndpointAndPassReceiver());

  base::RunLoop delete_loop;
  bool success = true;
  test::MockLevelDBObserver observer4;
  area_o1->AddObserver(observer4.Bind());
  area_o1->Delete(StringPieceToUint8Vector("key"), base::nullopt, "source",
                  base::BindLambdaForTesting([&](bool success_in) {
                    success = success_in;
                    delete_loop.Quit();
                  }));

  // And deleting the value from the new area should have failed (as the
  // database is empty).
  delete_loop.Run();
  area_o1.reset();
  ss_namespace.reset();
  context()->DeleteSessionNamespace(namespace_id, true);

  {
    // Committing data should now work.
    DoTestPut(namespace_id, origin1, "key", "value", "source");
    base::Optional<std::vector<uint8_t>> opt_value =
        DoTestGet(namespace_id, origin1, "key");
    ASSERT_TRUE(opt_value);
    EXPECT_EQ(StringPieceToUint8Vector("value"), opt_value.value());
  }
}

TEST_F(SessionStorageContextMojoTest, DontRecreateOnRepeatedCommitFailure) {
  std::string namespace_id = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  base::Optional<base::RunLoop> open_loop;
  size_t num_database_open_requests = 0;
  size_t num_databases_destroyed = 0;
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();
  }));
  open_loop.emplace();

  // Open three connections to the database.
  mojo::AssociatedRemote<blink::mojom::StorageArea> area;
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace;
  context()->CreateSessionNamespace(namespace_id);

  context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                GetBadMessageCallback(),
                                ss_namespace.BindNewPipeAndPassReceiver());
  ss_namespace->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());
  open_loop->Run();

  // Ensure that this database always fails to write data.
  context()->GetDatabaseForTesting().PostTaskWithThisObject(
      FROM_HERE,
      base::BindLambdaForTesting([&](storage::DomStorageDatabase* db) {
        db->MakeAllCommitsFailForTesting();
        db->SetDestructionCallbackForTesting(
            base::BindLambdaForTesting([&] { ++num_databases_destroyed; }));
      }));

  // Verify one attempt was made to open the database.
  EXPECT_EQ(1u, num_database_open_requests);

  // Setup a new RunLoop so we can wait until LocalStorageContextMojo tries to
  // reconnect to the database, which should happen after several commit
  // errors.
  open_loop.emplace();
  context()->SetDatabaseOpenCallbackForTesting(base::BindLambdaForTesting([&] {
    ++num_database_open_requests;
    open_loop->Quit();

    // Ensure that this database also always fails to write data.
    context()->GetDatabaseForTesting().Post(
        FROM_HERE, &storage::DomStorageDatabase::MakeAllCommitsFailForTesting);
  }));

  // Repeatedly write data to the database, to trigger enough commit errors.
  auto value = StringPieceToUint8Vector("avalue");
  base::Optional<std::vector<uint8_t>> old_value = base::nullopt;
  while (area.is_connected()) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringPieceToUint8Vector("key"), value, old_value, "source",
              base::BindLambdaForTesting(
                  [&](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushAreaForTesting(namespace_id, origin1);

    old_value = value;
    value[0]++;
  }
  area.reset();

  // Wait for LocalStorageContextMojo to try to reconnect to the database, and
  // connect that new request with a database implementation that always fails
  // on write.
  open_loop->Run();

  EXPECT_EQ(2u, num_database_open_requests);
  EXPECT_EQ(1u, num_databases_destroyed);

  // Reconnect a area to the database, and repeatedly write data to it again.
  // This time all should just keep getting written, and commit errors are
  // getting ignored.
  ss_namespace.reset();
  context()->OpenSessionStorage(kTestProcessId, namespace_id,
                                GetBadMessageCallback(),
                                ss_namespace.BindNewPipeAndPassReceiver());
  ss_namespace->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());

  old_value = base::nullopt;
  for (int i = 0; i < 64; ++i) {
    // Every write needs to be different to make sure there actually is a
    // change to commit.
    area->Put(StringPieceToUint8Vector("key"), value, old_value, "source",
              base::BindLambdaForTesting(
                  [&](bool success) { EXPECT_TRUE(success); }));
    area.FlushForTesting();
    RunUntilIdle();
    // And we need to flush after every change. Otherwise changes get batched up
    // and only one commit is done some time later.
    context()->FlushAreaForTesting(namespace_id, origin1);

    old_value = value;
    value[0]++;
  }

  // Should still be connected after all that.
  RunUntilIdle();
  EXPECT_TRUE(area.is_connected());

  context()->DeleteSessionNamespace(namespace_id, false);
  ShutdownContext();
}

TEST_F(SessionStorageContextMojoTest, GetUsage) {
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area;
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

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
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area;
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  context()->DeleteStorage(origin1, namespace_id1, base::DoNothing());

  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Next, test that it deletes the data even if there isn't a namespace open.
  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  area.reset();
  ss_namespace1.reset();

  // Delete the namespace and shutdown the context, BUT persist the namespace so
  // it can be loaded again.
  context()->DeleteSessionNamespace(namespace_id1, true);
  ShutdownContext();

  // This restarts the context, then deletes the storage.
  context()->DeleteStorage(origin1, namespace_id1, base::DoNothing());

  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());
  data.clear();
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, PurgeInactiveWrappers) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area;
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  context()->FlushAreaForTesting(namespace_id1, origin1);

  ss_namespace1.reset();
  area.reset();

  // Clear all the data from the backing database.
  base::RunLoop loop;
  context()->DatabaseForTesting()->DeletePrefixed(
      StringPieceToUint8Vector("map"),
      base::BindLambdaForTesting([&](leveldb::Status status) {
        loop.Quit();
        EXPECT_TRUE(status.ok());
      }));
  loop.Run();

  // Now open many new wrappers (for different origins) to trigger clean up.
  for (int i = 1; i <= 100; ++i) {
    context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                  GetBadMessageCallback(),
                                  ss_namespace1.BindNewPipeAndPassReceiver());
    ss_namespace1->OpenArea(url::Origin::Create(GURL(base::StringPrintf(
                                "http://example.com:%d", i))),
                            area.BindNewEndpointAndPassReceiver());
    RunUntilIdle();
    ss_namespace1.reset();
    area.reset();
  }

  // And make sure caches were actually cleared.
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());
  std::vector<blink::mojom::KeyValuePtr> data;
  ASSERT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

// TODO(https://crbug.com/1008697): Flakes when verifying no data found.
TEST_F(SessionStorageContextMojoTest, ClearDiskState) {
  SetBackingMode(SessionStorageContextMojo::BackingMode::kClearDiskStateOnOpen);
  std::string namespace_id1 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());

  mojo::AssociatedRemote<blink::mojom::StorageArea> area;
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());

  // Verify no data.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  area.reset();
  ss_namespace1.reset();

  // Delete the namespace and shutdown the context, BUT persist the namespace on
  // disk.
  context()->DeleteSessionNamespace(namespace_id1, true);
  ShutdownContext();

  // This will re-open the context, and load the persisted namespace, but it
  // should have been deleted due to our backing mode.
  context()->CreateSessionNamespace(namespace_id1);
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  ss_namespace1->OpenArea(origin1, area.BindNewEndpointAndPassReceiver());

  // The data from before should not be here, because the context clears disk
  // space on open.
  EXPECT_TRUE(test::GetAllSync(area.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, InterruptedCloneWithDelete) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->DeleteSessionNamespace(namespace_id1, false);

  // Open the second namespace which should be initialized and empty.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, InterruptedCloneChainWithDelete) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->CloneSessionNamespace(
      namespace_id2, namespace_id3,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->DeleteSessionNamespace(namespace_id2, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace3;
  context()->OpenSessionStorage(kTestProcessId, namespace_id3,
                                GetBadMessageCallback(),
                                ss_namespace3.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n3;
  ss_namespace3->OpenArea(origin1, area_n3.BindNewEndpointAndPassReceiver());

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n3.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, InterruptedTripleCloneChain) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  std::string namespace_id4 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->CloneSessionNamespace(
      namespace_id2, namespace_id3,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->CloneSessionNamespace(
      namespace_id3, namespace_id4,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->DeleteSessionNamespace(namespace_id3, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace4;
  context()->OpenSessionStorage(kTestProcessId, namespace_id4,
                                GetBadMessageCallback(),
                                ss_namespace4.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n4;
  ss_namespace4->OpenArea(origin1, area_n4.BindNewEndpointAndPassReceiver());

  // Trigger the populated of namespace 2 by deleting namespace 1.
  context()->DeleteSessionNamespace(namespace_id1, false);

  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n4.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, TotalCloneChainDeletion) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  std::string namespace_id3 = base::GenerateGUID();
  std::string namespace_id4 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);

  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->CloneSessionNamespace(
      namespace_id2, namespace_id3,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->CloneSessionNamespace(
      namespace_id3, namespace_id4,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  context()->DeleteSessionNamespace(namespace_id2, false);
  context()->DeleteSessionNamespace(namespace_id3, false);
  context()->DeleteSessionNamespace(namespace_id1, false);
  context()->DeleteSessionNamespace(namespace_id4, false);
}

}  // namespace

TEST_F(SessionStorageContextMojoTest, PurgeMemoryDoesNotCrashOrHang) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));

  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  context()->CreateSessionNamespace(namespace_id2);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  // Put some data in both.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));
  EXPECT_TRUE(test::PutSync(area_n2.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value2"), base::nullopt,
                            "source1"));

  context()->FlushAreaForTesting(namespace_id1, origin1);

  area_n2.reset();
  ss_namespace2.reset();

  RunUntilIdle();

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
  EXPECT_TRUE(test::GetAllSync(area_n1.get(), &data));
  EXPECT_EQ(1ul, data.size());

  base::Optional<std::vector<uint8_t>> opt_value2 =
      DoTestGet(namespace_id2, origin1, "key1");
  ASSERT_TRUE(opt_value2);
  EXPECT_EQ(StringPieceToUint8Vector("value2"), opt_value2.value());
}

TEST_F(SessionStorageContextMojoTest, DeleteWithPersistBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Delete the origin namespace, but save it.
  context()->DeleteSessionNamespace(namespace_id1, true);

  // Do the browser-side clone.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  // The data should be in namespace 2.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, DeleteWithoutPersistBeforeBrowserClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Delete the origin namespace and don't save it.
  context()->DeleteSessionNamespace(namespace_id1, false);

  // Do the browser-side clone.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  // The data should be gone, because the first namespace wasn't saved to disk.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(0ul, data.size());
}

TEST_F(SessionStorageContextMojoTest, DeleteAfterCloneWithoutMojoClone) {
  std::string namespace_id1 = base::GenerateGUID();
  std::string namespace_id2 = base::GenerateGUID();
  url::Origin origin1 = url::Origin::Create(GURL("http://foobar.com"));
  context()->CreateSessionNamespace(namespace_id1);
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace1;
  context()->OpenSessionStorage(kTestProcessId, namespace_id1,
                                GetBadMessageCallback(),
                                ss_namespace1.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n1;
  ss_namespace1->OpenArea(origin1, area_n1.BindNewEndpointAndPassReceiver());

  // Put some data.
  EXPECT_TRUE(test::PutSync(area_n1.get(), StringPieceToUint8Vector("key1"),
                            StringPieceToUint8Vector("value1"), base::nullopt,
                            "source1"));

  // Do the browser-side clone.
  context()->CloneSessionNamespace(
      namespace_id1, namespace_id2,
      SessionStorageContextMojo::CloneType::kWaitForCloneOnNamespace);

  // Delete the origin namespace and don't save it.
  context()->DeleteSessionNamespace(namespace_id1, false);

  // Open the second namespace.
  mojo::Remote<blink::mojom::SessionStorageNamespace> ss_namespace2;
  context()->OpenSessionStorage(kTestProcessId, namespace_id2,
                                GetBadMessageCallback(),
                                ss_namespace2.BindNewPipeAndPassReceiver());
  mojo::AssociatedRemote<blink::mojom::StorageArea> area_n2;
  ss_namespace2->OpenArea(origin1, area_n2.BindNewEndpointAndPassReceiver());

  // The data should be there, as the namespace should clone to all pending
  // namespaces on destruction if it didn't get a 'Clone' from mojo.
  std::vector<blink::mojom::KeyValuePtr> data;
  EXPECT_TRUE(test::GetAllSync(area_n2.get(), &data));
  EXPECT_EQ(1ul, data.size());
}

}  // namespace content
