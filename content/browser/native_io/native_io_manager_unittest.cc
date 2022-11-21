// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "components/services/storage/public/cpp/buckets/bucket_info.h"
#include "components/services/storage/public/cpp/buckets/constants.h"
#include "components/services/storage/public/cpp/quota_error_or.h"
#include "content/browser/native_io/native_io_manager.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/fake_message_dispatch_context.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/quota_manager_proxy_sync.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "url/origin.h"

using ::blink::StorageKey;
using ::blink::mojom::NativeIOErrorPtr;
using ::blink::mojom::NativeIOErrorType;

namespace content {

namespace {

// Synchronous proxies to a wrapped NativeIOManager's methods.
class NativeIOManagerSync {
 public:
  // The caller must ensure that the NativeIOManager outlives this.
  explicit NativeIOManagerSync(NativeIOManager* io_manager)
      : io_manager_(io_manager) {}

  NativeIOManagerSync(const NativeIOManagerSync&) = delete;
  NativeIOManagerSync& operator=(const NativeIOManagerSync&) = delete;

  blink::mojom::QuotaStatusCode DeleteStorageKeyData(
      const StorageKey& storage_key) {
    blink::mojom::QuotaStatusCode success_code;
    base::RunLoop run_loop;
    io_manager_->DeleteStorageKeyData(
        storage_key, base::BindLambdaForTesting(
                         [&](blink::mojom::QuotaStatusCode returned_status) {
                           success_code = returned_status;
                           run_loop.Quit();
                         }));
    run_loop.Run();
    return success_code;
  }

  std::vector<StorageKey> GetStorageKeysForType(
      blink::mojom::StorageType type) {
    std::vector<StorageKey> storage_keys;
    base::RunLoop run_loop;
    io_manager_->GetStorageKeysForType(
        type, base::BindLambdaForTesting(
                  [&](const std::vector<StorageKey>& returned_storage_keys) {
                    storage_keys = returned_storage_keys;
                    run_loop.Quit();
                  }));
    run_loop.Run();
    return storage_keys;
  }

  int64_t GetStorageKeyUsage(const StorageKey& storage_key,
                             blink::mojom::StorageType type) {
    int64_t usage;
    base::RunLoop run_loop;
    io_manager_->GetStorageKeyUsage(
        storage_key, type,
        base::BindLambdaForTesting([&](int64_t returned_usage) {
          usage = returned_usage;
          run_loop.Quit();
        }));
    run_loop.Run();
    return usage;
  }

 private:
  const raw_ptr<NativeIOManager> io_manager_;
};

struct OpenFileResult {
  base::File file;
  uint64_t file_size;
  NativeIOErrorPtr error;
};

#if BUILDFLAG(IS_MAC)
struct SetLengthResult {
  base::File file;
  int64_t actual_length;
  NativeIOErrorPtr error;
};
#endif  // BUILDFLAG(IS_MAC)

// Synchronous proxies to a wrapped NativeIOHost's methods.
class NativeIOHostSync {
 public:
  // The caller must ensure that the NativeIOHost outlives this.
  explicit NativeIOHostSync(blink::mojom::NativeIOHost* io_host)
      : io_host_(io_host) {}

  NativeIOHostSync(const NativeIOHostSync&) = delete;
  NativeIOHostSync& operator=(const NativeIOHostSync&) = delete;

  ~NativeIOHostSync() = default;

  OpenFileResult OpenFile(
      const std::string& name,
      mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_receiver) {
    OpenFileResult result;
    base::RunLoop run_loop;
    io_host_->OpenFile(name, std::move(file_receiver),
                       base::BindLambdaForTesting(
                           [&](base::File backend_file, uint64_t file_size,
                               NativeIOErrorPtr open_error) {
                             result.file = std::move(backend_file);
                             result.file_size = file_size;
                             result.error = std::move(open_error);
                             run_loop.Quit();
                           }));
    run_loop.Run();
    return result;
  }

  std::pair<NativeIOErrorPtr, uint64_t> DeleteFile(const std::string& name) {
    NativeIOErrorPtr error;
    uint64_t deleted_size;
    base::RunLoop run_loop;
    io_host_->DeleteFile(
        name, base::BindLambdaForTesting([&](NativeIOErrorPtr delete_error,
                                             uint64_t deleted_file_size) {
          error = std::move(delete_error);
          deleted_size = deleted_file_size;
          run_loop.Quit();
        }));
    run_loop.Run();
    return {std::move(error), deleted_size};
  }

  std::vector<std::string> GetAllFileNames() {
    bool success;
    std::vector<std::string> names;
    base::RunLoop run_loop;
    io_host_->GetAllFileNames(base::BindLambdaForTesting(
        [&](bool backend_success,
            const std::vector<std::string>& backend_names) {
          success = backend_success;
          names = backend_names;
          run_loop.Quit();
        }));
    run_loop.Run();
    return names;
  }

  NativeIOErrorPtr RenameFile(const std::string& old_name,
                              const std::string& new_name) {
    base::RunLoop run_loop;
    NativeIOErrorPtr error;
    io_host_->RenameFile(
        old_name, new_name,
        base::BindLambdaForTesting([&](NativeIOErrorPtr rename_error) {
          error = std::move(rename_error);
          run_loop.Quit();
        }));
    run_loop.Run();
    return error;
  }

 private:
  const raw_ptr<blink::mojom::NativeIOHost> io_host_;
};

// Synchronous proxies to a wrapped NativeIOFileHost's methods.
class NativeIOFileHostSync {
 public:
  // The caller must ensure that the NativeIOFileHost outlives this.
  explicit NativeIOFileHostSync(blink::mojom::NativeIOFileHost* file_host)
      : file_host_(file_host) {}

  NativeIOFileHostSync(const NativeIOFileHostSync&) = delete;
  NativeIOFileHostSync& operator=(const NativeIOFileHostSync&) = delete;

  ~NativeIOFileHostSync() = default;

  void Close() {
    base::RunLoop run_loop;
    file_host_->Close(run_loop.QuitClosure());
    run_loop.Run();
    return;
  }

#if BUILDFLAG(IS_MAC)
  SetLengthResult SetLength(const int64_t length, base::File file) {
    SetLengthResult result;
    base::RunLoop run_loop;
    file_host_->SetLength(
        length, std::move(file),
        base::BindLambdaForTesting([&](base::File backend_file,
                                       int64_t actual_length,
                                       NativeIOErrorPtr set_length_error) {
          result.file = std::move(backend_file);
          result.actual_length = actual_length;
          result.error = std::move(set_length_error);
          run_loop.Quit();
        }));
    run_loop.Run();
    return result;
  }
#endif  // BUILDFLAG(IS_MAC)

 private:
  const raw_ptr<blink::mojom::NativeIOFileHost> file_host_;
};

const char kExampleStorageKey[] = "https://example.com";
const char kGoogleStorageKey[] = "https://google.com";

class NativeIOManagerTest : public testing::TestWithParam<bool> {
 public:
  NativeIOManagerTest()
      : special_storage_policy_(
            base::MakeRefCounted<storage::MockSpecialStoragePolicy>()) {}
  ~NativeIOManagerTest() override = default;

  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::SingleThreadTaskRunner::GetCurrentDefault(),
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager(), base::SingleThreadTaskRunner::GetCurrentDefault());
    manager_ = std::make_unique<NativeIOManager>(data_dir_.GetPath(),
#if BUILDFLAG(IS_MAC)
                                                 allow_set_length_ipc(),
#endif  // BUILDFLAG(IS_MAC)
                                                 special_storage_policy_,
                                                 quota_manager_proxy_);

    manager_->BindReceiver(
        StorageKey::CreateFromStringForTesting(kExampleStorageKey),
        example_host_remote_.BindNewPipeAndPassReceiver(),
        GetBadMessageCallback());
    manager_->BindReceiver(
        StorageKey::CreateFromStringForTesting(kGoogleStorageKey),
        google_host_remote_.BindNewPipeAndPassReceiver(),
        GetBadMessageCallback());

    sync_manager_ =
        std::make_unique<NativeIOManagerSync>(std::move(manager_.get()));

    example_host_ =
        std::make_unique<NativeIOHostSync>(example_host_remote_.get());
    google_host_ =
        std::make_unique<NativeIOHostSync>(google_host_remote_.get());
  }

  void TearDown() override {
    // Let the client go away before dropping a ref of the quota manager proxy.
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;
  }

  storage::MockQuotaManager* quota_manager() {
    return static_cast<storage::MockQuotaManager*>(quota_manager_.get());
  }

  storage::MockQuotaManagerProxy* quota_manager_proxy() {
    return static_cast<storage::MockQuotaManagerProxy*>(
        quota_manager_proxy_.get());
  }

  mojo::ReportBadMessageCallback GetBadMessageCallback() {
    return base::BindOnce(&NativeIOManagerTest::OnBadMessage,
                          base::Unretained(this));
  }

  void OnBadMessage(base::StringPiece reason) { NOTREACHED(); }

  bool allow_set_length_ipc() { return GetParam(); }

 protected:
  struct Filename {
    std::string name;
    bool valid;
  };

  const std::vector<Filename> filenames_ = {
      {"ascii", true},
      {"_underscores_", true},
      {std::string(99, 'x'), true},
      {std::string(100, 'x'), true},
      {"Uppercase", false},
      {"Uppercase", false},
      {"has-dash", false},
      {"has.dot", false},
      {"has/slash", false},
      {std::string(101, 'x'), false},
      {std::string(9999, 'x'), false},
  };

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;

  // This must be above NativeIOManager, to ensure that no file is accessed when
  // the temporary directory is deleted.
  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because NativeIOHost uses the
  // thread pool for file I/O.
  base::test::TaskEnvironment task_environment_;

  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;

  // The NativeIOManager is on the heap because it requires the profile path at
  // construction, and we only know the path during SetUp.
  std::unique_ptr<NativeIOManager> manager_;

  std::unique_ptr<NativeIOManagerSync> sync_manager_;

  // Hosts for two different storage_keys, used for isolation testing.
  mojo::Remote<blink::mojom::NativeIOHost> example_host_remote_;
  mojo::Remote<blink::mojom::NativeIOHost> google_host_remote_;
  std::unique_ptr<NativeIOHostSync> example_host_;
  std::unique_ptr<NativeIOHostSync> google_host_;
};

TEST_P(NativeIOManagerTest, DefaultBucketCreatedOnBindReceiver) {
  EXPECT_THAT(google_host_->GetAllFileNames(), testing::SizeIs(0));
  storage::QuotaManagerProxySync quota_manager_proxy_sync(
      quota_manager_proxy());

  // Check default bucket exists for https://example.com.
  storage::QuotaErrorOr<storage::BucketInfo> result =
      quota_manager_proxy_sync.GetBucket(
          StorageKey::CreateFromStringForTesting(kExampleStorageKey),
          storage::kDefaultBucketName, blink::mojom::StorageType::kTemporary);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->name, storage::kDefaultBucketName);
  EXPECT_EQ(result->storage_key,
            StorageKey::CreateFromStringForTesting(kExampleStorageKey));
  EXPECT_GT(result->id.value(), 0);

  // Check default bucket exists for https://google.com.
  result = quota_manager_proxy_sync.GetBucket(
      StorageKey::CreateFromStringForTesting(kGoogleStorageKey),
      storage::kDefaultBucketName, blink::mojom::StorageType::kTemporary);
  EXPECT_TRUE(result.ok());
  EXPECT_EQ(result->name, storage::kDefaultBucketName);
  EXPECT_EQ(result->storage_key,
            StorageKey::CreateFromStringForTesting(kGoogleStorageKey));
  EXPECT_GT(result->id.value(), 0);
}

TEST_P(NativeIOManagerTest, OpenFile_Names) {
  for (const Filename& filename : filenames_) {
    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
    OpenFileResult result = example_host_->OpenFile(
        filename.name, file_host.BindNewPipeAndPassReceiver());
    EXPECT_EQ(result.file.IsValid(), filename.valid);

    if (!filename.valid) {
      EXPECT_EQ(result.file_size, 0u);
      EXPECT_EQ(result.error->type, NativeIOErrorType::kUnknown);
      EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
    }
  }
}

TEST_P(NativeIOManagerTest, OpenFile_Locks_OpenFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());

  mojo::Remote<blink::mojom::NativeIOFileHost> locked_file_host_remote;
  OpenFileResult locked_result = example_host_->OpenFile(
      "test_file", locked_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(locked_result.file.IsValid());
  EXPECT_EQ(locked_result.error->type,
            NativeIOErrorType::kNoModificationAllowed)
      << "A file cannot be opened twice";
}

TEST_P(NativeIOManagerTest, OpenFile_SameName) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  OpenFileResult same_result = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(same_result.file.IsValid());
  EXPECT_EQ(same_result.file_size, static_cast<uint64_t>(kTestData.size()));
  char read_buffer[kTestData.size()];
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            same_result.file.Read(0, read_buffer, kTestData.size()));
  EXPECT_EQ(kTestData, std::string(read_buffer, kTestData.size()));
  same_result.file.Close();
}

TEST_P(NativeIOManagerTest, DeleteFile_Names) {
  for (const Filename& filename : filenames_) {
    if (filename.valid) {
      EXPECT_EQ(example_host_->DeleteFile(filename.name).first->type,
                NativeIOErrorType::kSuccess);
    } else {
      mojo::test::BadMessageObserver bad_message_observer;
      EXPECT_EQ(example_host_->DeleteFile(filename.name).first->type,
                NativeIOErrorType::kUnknown);
      EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
    }
  }
}

TEST_P(NativeIOManagerTest, OpenFile_Locks_DeleteFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());

  EXPECT_EQ(example_host_->DeleteFile("test_file").first->type,
            NativeIOErrorType::kNoModificationAllowed);
}

TEST_P(NativeIOManagerTest, OpenFile_Locks_RenameFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  base::File file =
      example_host_
          ->OpenFile("test_file_in_use", file_host.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host2;
  base::File file_closed =
      example_host_
          ->OpenFile("test_file_closed",
                     file_host2.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file_closed.IsValid());
  file_closed.Close();
  NativeIOFileHostSync file_host2_sync(file_host2.get());
  file_host2_sync.Close();

  EXPECT_EQ(
      example_host_->RenameFile("test_file_in_use", "renamed_test_file")->type,
      NativeIOErrorType::kNoModificationAllowed)
      << "An open file cannot be renamed";

  EXPECT_EQ(
      example_host_->RenameFile("test_file_closed", "test_file_in_use")->type,
      NativeIOErrorType::kNoModificationAllowed)
      << "An open file cannot be overwritten";
  ;
}

TEST_P(NativeIOManagerTest, DeleteFile_WipesData) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  EXPECT_EQ(example_host_->DeleteFile("test_file").first->type,
            NativeIOErrorType::kSuccess);

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  base::File same_file =
      example_host_
          ->OpenFile("test_file",
                     same_file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(0, same_file.Read(0, read_buffer, kTestData.size()));
}

TEST_P(NativeIOManagerTest, DeleteFile_ReportsLengths) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  OpenFileResult same_file_result = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(same_file_result.file.IsValid());
  ASSERT_EQ(same_file_result.file_size,
            static_cast<uint64_t>(kTestData.size()));
  EXPECT_EQ(same_file_result.error->type, NativeIOErrorType::kSuccess);
  same_file_result.file.Close();
  NativeIOFileHostSync same_file_host(same_file_host_remote.get());
  same_file_host.Close();

  std::pair<NativeIOErrorPtr, uint64_t> delete_result =
      example_host_->DeleteFile("test_file");

  EXPECT_EQ(delete_result.first->type, NativeIOErrorType::kSuccess);
  EXPECT_EQ(delete_result.second, static_cast<uint64_t>(kTestData.size()));
}

TEST_P(NativeIOManagerTest, GetAllFiles_Empty) {
  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(0u, file_names.size());
}

TEST_P(NativeIOManagerTest, GetAllFiles_AfterOpen) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());
  EXPECT_EQ("test_file", file_names[0]);
}

TEST_P(NativeIOManagerTest, RenameFile_AfterOpenAndRename) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  example_host_->RenameFile("test_file", "renamed_test_file");
  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());
  EXPECT_EQ("renamed_test_file", file_names[0]);
}

TEST_P(NativeIOManagerTest, RenameFile_Names) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  for (const Filename& filename : filenames_) {
    if (filename.valid) {
      EXPECT_EQ(example_host_->RenameFile("test_file", filename.name)->type,
                NativeIOErrorType::kSuccess);
      EXPECT_EQ(example_host_->RenameFile(filename.name, "inexistant_test_file")
                    ->type,
                NativeIOErrorType::kSuccess);

      // Return to initial state
      EXPECT_EQ(
          example_host_->RenameFile("inexistant_test_file", "test_file")->type,
          NativeIOErrorType::kSuccess);
    } else {
      mojo::test::BadMessageObserver bad_message_observer;

      EXPECT_EQ(example_host_->RenameFile("test_file", filename.name)->type,
                NativeIOErrorType::kUnknown);
      EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());

      EXPECT_EQ(example_host_->RenameFile(filename.name, "inexistant_test_file")
                    ->type,
                NativeIOErrorType::kUnknown);
      EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
    }
  }
}

#if BUILDFLAG(IS_MAC)
TEST_P(NativeIOManagerTest, SetLength) {
  const std::string kTestData("Test Data");
  const int kTestDataSize = kTestData.size();
  const int kTruncatedSize = 4;

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(kTestDataSize, file.Write(0, kTestData.data(), kTestDataSize));

  NativeIOFileHostSync file_host(file_host_remote.get());

  SetLengthResult set_length_result;

  if (allow_set_length_ipc()) {
    set_length_result = file_host.SetLength(kTruncatedSize, std::move(file));
    EXPECT_EQ(set_length_result.error->type, NativeIOErrorType::kSuccess);
    EXPECT_EQ(set_length_result.actual_length, kTruncatedSize);
  } else {
    mojo::test::BadMessageObserver bad_message_observer;
    set_length_result = file_host.SetLength(kTruncatedSize, std::move(file));
    EXPECT_EQ(set_length_result.error->type, NativeIOErrorType::kUnknown);
    EXPECT_EQ("SetLength() disabled on this OS.",
              bad_message_observer.WaitForBadMessage());
    EXPECT_EQ(set_length_result.actual_length, 0);
  }

  file = std::move(set_length_result.file);
  EXPECT_TRUE(file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(allow_set_length_ipc() ? kTruncatedSize : kTestDataSize,
            file.Read(0, read_buffer, kTestData.size()));
}

TEST_P(NativeIOManagerTest, SetLength_NegativeLength) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      example_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  NativeIOFileHostSync file_host(file_host_remote.get());

  mojo::test::BadMessageObserver bad_message_observer;
  SetLengthResult set_length_result = file_host.SetLength(-5, std::move(file));
  EXPECT_EQ(set_length_result.error->type, NativeIOErrorType::kUnknown);
  EXPECT_EQ(allow_set_length_ipc() ? "The file length cannot be negative."
                                   : "SetLength() disabled on this OS.",
            bad_message_observer.WaitForBadMessage());
}
#endif  // BUILDFLAG(IS_MAC)

TEST_P(NativeIOManagerTest, StorageKeyIsolation) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file =
      google_host_
          ->OpenFile("test_file", file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  std::vector<std::string> file_names = google_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());

  std::vector<std::string> other_names = example_host_->GetAllFileNames();
  EXPECT_EQ(0u, other_names.size());

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  base::File same_file =
      example_host_
          ->OpenFile("test_file",
                     same_file_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(0, same_file.Read(0, read_buffer, kTestData.size()));
}

TEST_P(NativeIOManagerTest, BindReceiver_UntrustworthyStorageKey) {
  mojo::Remote<blink::mojom::NativeIOHost> insecure_host_remote_;

  mojo::FakeMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  manager_->BindReceiver(
      StorageKey::CreateFromStringForTesting("http://insecure.com"),
      insecure_host_remote_.BindNewPipeAndPassReceiver(),
      mojo::GetBadMessageCallback());
  EXPECT_EQ("Called NativeIO from an insecure context",
            bad_message_observer.WaitForBadMessage());
}

TEST_P(NativeIOManagerTest, DeleteStorageKeyData_UnsupportedStorageKey) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(example_file.IsValid());
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();

  StorageKey insecure_storage_key =
      StorageKey::CreateFromStringForTesting("http://insecure.com");

  EXPECT_EQ(sync_manager_->DeleteStorageKeyData(insecure_storage_key),
            blink::mojom::QuotaStatusCode::kOk);

  EXPECT_TRUE(base::PathExists(manager_->RootPathForStorageKey(
      StorageKey::CreateFromStringForTesting(kExampleStorageKey))));
}

TEST_P(NativeIOManagerTest, DeleteStorageKeyData_StorageKeyWithNoData) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;
  EXPECT_TRUE(example_file.IsValid());
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();

  StorageKey storage_key_with_no_data =
      StorageKey::CreateFromStringForTesting("https://other.example.com");

  EXPECT_EQ(sync_manager_->DeleteStorageKeyData(storage_key_with_no_data),
            blink::mojom::QuotaStatusCode::kOk);

  EXPECT_TRUE(base::PathExists(manager_->RootPathForStorageKey(
      StorageKey::CreateFromStringForTesting(kExampleStorageKey))));
}

TEST_P(NativeIOManagerTest, DeleteStorageKeyData_ConcurrentDeletion) {
  {
    mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
    base::File example_file =
        example_host_
            ->OpenFile("test_file",
                       example_host_remote.BindNewPipeAndPassReceiver())
            .file;
    EXPECT_TRUE(example_file.IsValid());
    example_file.Close();
    NativeIOFileHostSync example_file_host(example_host_remote.get());
    example_file_host.Close();
  }

  // Reset the last mojo connection to the example host, so the host remains
  // without connections during deletion.
  example_host_ = nullptr;
  example_host_remote_.reset();

  StorageKey example_storage_key =
      StorageKey::CreateFromStringForTesting(kExampleStorageKey);

  base::RunLoop delete_run_loop;
  blink::mojom::QuotaStatusCode delete_status;
  manager_->DeleteStorageKeyData(
      example_storage_key,
      base::BindLambdaForTesting([&](blink::mojom::QuotaStatusCode status) {
        delete_run_loop.Quit();
        delete_status = status;
      }));

  EXPECT_EQ(sync_manager_->DeleteStorageKeyData(example_storage_key),
            blink::mojom::QuotaStatusCode::kOk);

  delete_run_loop.Run();
  EXPECT_EQ(delete_status, blink::mojom::QuotaStatusCode::kOk);

  EXPECT_TRUE(
      !base::PathExists(manager_->RootPathForStorageKey(example_storage_key)));
}

TEST_P(NativeIOManagerTest, GetStorageKeysByType_Empty) {
  std::vector<StorageKey> storage_keys = sync_manager_->GetStorageKeysForType(
      blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(0u, storage_keys.size());
}

TEST_P(NativeIOManagerTest, GetStorageKeysByType_ReturnsInactiveStorageKeys) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();

  std::vector<StorageKey> storage_keys = sync_manager_->GetStorageKeysForType(
      blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(1u, storage_keys.size());
  EXPECT_EQ(StorageKey::CreateFromStringForTesting(kExampleStorageKey),
            storage_keys[0]);
}

TEST_P(NativeIOManagerTest, GetStorageKeysByType_ReturnsActiveStorageKeys) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;

  std::vector<StorageKey> storage_keys = sync_manager_->GetStorageKeysForType(
      blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(1u, storage_keys.size());
  EXPECT_EQ(StorageKey::CreateFromStringForTesting(kExampleStorageKey),
            storage_keys[0]);

  EXPECT_TRUE(example_file.IsValid());
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();
}

TEST_P(NativeIOManagerTest, GetStorageKeyUsage_ActiveStorageKeyUsage) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;
  int64_t expected_usage = 100;
  example_file.SetLength(expected_usage);

  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();

  int64_t usage = sync_manager_->GetStorageKeyUsage(
      StorageKey::CreateFromStringForTesting(kExampleStorageKey),
      blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(expected_usage, usage);
}

TEST_P(NativeIOManagerTest, GetStorageKeyUsage_NonexistingStorageKeyUsage) {
  int64_t usage = sync_manager_->GetStorageKeyUsage(
      StorageKey::CreateFromStringForTesting(kExampleStorageKey),
      blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(0u, usage);
}

TEST_P(NativeIOManagerTest, IncognitoQuota) {
  auto quota_manager = base::MakeRefCounted<storage::MockQuotaManager>(
      /*is_incognito=*/true, base::FilePath(),
      base::SingleThreadTaskRunner::GetCurrentDefault().get(),
      special_storage_policy_);
  auto quota_manager_proxy =
      base::MakeRefCounted<storage::MockQuotaManagerProxy>(
          quota_manager.get(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
  auto manager = std::make_unique<NativeIOManager>(base::FilePath(),
#if BUILDFLAG(IS_MAC)
                                                   allow_set_length_ipc(),
#endif  // BUILDFLAG(IS_MAC)
                                                   special_storage_policy_,
                                                   quota_manager_proxy);
  auto sync_manager = std::make_unique<NativeIOManagerSync>(manager.get());

  EXPECT_THAT(sync_manager->GetStorageKeysForType(
                  blink::mojom::StorageType::kTemporary),
              testing::SizeIs(0));
  EXPECT_EQ(0, sync_manager->GetStorageKeyUsage(
                   StorageKey::CreateFromStringForTesting(kExampleStorageKey),
                   blink::mojom::StorageType::kTemporary));
}

INSTANTIATE_TEST_SUITE_P(,
                         NativeIOManagerTest,
                         ::testing::Values(
#if BUILDFLAG(IS_MAC)
                             false,
                             true
#else   // !BUILDFLAG(IS_MAC)
                             false
#endif  // BUILDFLAG(IS_MAC)
                             ));

}  // namespace

}  // namespace content
