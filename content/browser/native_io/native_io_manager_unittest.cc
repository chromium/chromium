// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "content/browser/native_io/native_io_manager.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::NativeIOErrorPtr;
using blink::mojom::NativeIOErrorType;

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

  blink::mojom::QuotaStatusCode DeleteOriginData(const url::Origin& origin) {
    blink::mojom::QuotaStatusCode success_code;
    base::RunLoop run_loop;
    io_manager_->DeleteOriginData(
        origin, base::BindLambdaForTesting(
                    [&](blink::mojom::QuotaStatusCode returned_status) {
                      success_code = returned_status;
                      run_loop.Quit();
                    }));
    run_loop.Run();
    return success_code;
  }

  std::vector<url::Origin> GetOriginsForType(blink::mojom::StorageType type) {
    std::vector<url::Origin> origins;
    base::RunLoop run_loop;
    io_manager_->GetOriginsForType(
        type, base::BindLambdaForTesting(
                  [&](const std::vector<url::Origin>& returned_origins) {
                    origins = returned_origins;
                    run_loop.Quit();
                  }));
    run_loop.Run();
    return origins;
  }

  std::vector<url::Origin> GetOriginsForHost(blink::mojom::StorageType type,
                                             const std::string& host) {
    std::vector<url::Origin> origins;
    base::RunLoop run_loop;
    io_manager_->GetOriginsForHost(
        type, host,
        base::BindLambdaForTesting(
            [&](const std::vector<url::Origin>& returned_origins) {
              origins = returned_origins;
              run_loop.Quit();
            }));
    run_loop.Run();
    return origins;
  }

  int64_t GetOriginUsage(const url::Origin& origin,
                         blink::mojom::StorageType type) {
    int64_t usage;
    base::RunLoop run_loop;
    io_manager_->GetOriginUsage(
        origin, type, base::BindLambdaForTesting([&](int64_t returned_usage) {
          usage = returned_usage;
          run_loop.Quit();
        }));
    run_loop.Run();
    return usage;
  }

 private:
  NativeIOManager* const io_manager_;
};

struct OpenFileResult {
  base::File file;
  uint64_t file_size;
  NativeIOErrorPtr error;
};

#if defined(OS_MAC)
struct SetLengthResult {
  base::File file;
  int64_t actual_length;
  NativeIOErrorPtr error;
};
#endif  // defined(OS_MAC)

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
  blink::mojom::NativeIOHost* const io_host_;
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

#if defined(OS_MAC)
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
#endif  // defined(OS_MAC)

 private:
  blink::mojom::NativeIOFileHost* const file_host_;
};

const char kExampleOrigin[] = "https://example.com";
const char kGoogleOrigin[] = "https://google.com";

class NativeIOManagerTest : public testing::TestWithParam<bool> {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(),
        /*special storage policy=*/nullptr);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager(), base::ThreadTaskRunnerHandle::Get());
    manager_ = std::make_unique<NativeIOManager>(
        data_dir_.GetPath(),
#if defined(OS_MAC)
        allow_set_length_ipc(),
#endif  // defined(OS_MAC)
        /*special storage policy=*/nullptr, quota_manager_proxy());

    manager_->BindReceiver(url::Origin::Create(GURL(kExampleOrigin)),
                           example_host_remote_.BindNewPipeAndPassReceiver(),
                           GetBadMessageCallback());
    manager_->BindReceiver(url::Origin::Create(GURL(kGoogleOrigin)),
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
    quota_manager_proxy()->SimulateQuotaManagerDestroyed();

    // Let the client go away before dropping a ref of the quota manager proxy.
    quota_manager_ = nullptr;
    quota_manager_proxy_ = nullptr;
  }

 protected:
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

  void OnBadMessage(const std::string& reason) { NOTREACHED(); }

  // This must be above NativeIOManager, to ensure that no file is accessed when
  // the temporary directory is deleted.
  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because NativeIOHost uses the
  // thread pool for file I/O.
  base::test::TaskEnvironment task_environment_;

  // The NativeIOManager is on the heap because it requires the profile path at
  // construction, and we only know the path during SetUp.
  std::unique_ptr<NativeIOManager> manager_;

  std::unique_ptr<NativeIOManagerSync> sync_manager_;

  // Hosts for two different origins, used for isolation testing.
  mojo::Remote<blink::mojom::NativeIOHost> example_host_remote_;
  mojo::Remote<blink::mojom::NativeIOHost> google_host_remote_;
  std::unique_ptr<NativeIOHostSync> example_host_;
  std::unique_ptr<NativeIOHostSync> google_host_;

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

  bool allow_set_length_ipc() { return GetParam(); }

 private:
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
};

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

#if defined(OS_MAC)
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
#endif  // defined(OS_MAC)

TEST_P(NativeIOManagerTest, OriginIsolation) {
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

TEST_P(NativeIOManagerTest, BindReceiver_UntrustworthyOrigin) {
  mojo::Remote<blink::mojom::NativeIOHost> insecure_host_remote_;

  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  manager_->BindReceiver(url::Origin::Create(GURL("http://insecure.com")),
                         insecure_host_remote_.BindNewPipeAndPassReceiver(),
                         mojo::GetBadMessageCallback());
  EXPECT_EQ("Called NativeIO from an insecure context",
            bad_message_observer.WaitForBadMessage());
}

TEST_P(NativeIOManagerTest, DeleteOriginData_UnsupportedOrigin) {
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

  url::Origin insecure_origin =
      url::Origin::Create(GURL("http://insecure.com"));

  EXPECT_EQ(sync_manager_->DeleteOriginData(insecure_origin),
            blink::mojom::QuotaStatusCode::kOk);

  EXPECT_TRUE(base::PathExists(
      manager_->RootPathForOrigin(url::Origin::Create(GURL(kExampleOrigin)))));
}

TEST_P(NativeIOManagerTest, DeleteOriginData_OriginWithNoData) {
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

  url::Origin origin_with_no_data =
      url::Origin::Create(GURL("https://other.example.com"));

  EXPECT_EQ(sync_manager_->DeleteOriginData(origin_with_no_data),
            blink::mojom::QuotaStatusCode::kOk);

  EXPECT_TRUE(base::PathExists(
      manager_->RootPathForOrigin(url::Origin::Create(GURL(kExampleOrigin)))));
}

TEST_P(NativeIOManagerTest, DeleteOriginData_ConcurrentDeletion) {
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

  url::Origin example_origin = url::Origin::Create(GURL(kExampleOrigin));

  manager_->DeleteOriginData(
      example_origin, base::BindLambdaForTesting(
                          [&](blink::mojom::QuotaStatusCode returned_status) {
                            EXPECT_EQ(returned_status,
                                      blink::mojom::QuotaStatusCode::kOk);
                          }));

  EXPECT_EQ(sync_manager_->DeleteOriginData(example_origin),
            blink::mojom::QuotaStatusCode::kOk);

  EXPECT_TRUE(!base::PathExists(manager_->RootPathForOrigin(example_origin)));
}

TEST_P(NativeIOManagerTest, GetOriginsByType_Empty) {
  std::vector<url::Origin> origins =
      sync_manager_->GetOriginsForType(blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(0u, origins.size());
}

TEST_P(NativeIOManagerTest, GetOriginsByType_ReturnsInactiveOrigins) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();

  std::vector<url::Origin> origins =
      sync_manager_->GetOriginsForType(blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(url::Origin::Create(GURL(kExampleOrigin)), origins[0]);
}

TEST_P(NativeIOManagerTest, GetOriginsByType_ReturnsActiveOrigins) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;

  std::vector<url::Origin> origins =
      sync_manager_->GetOriginsForType(blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(1u, origins.size());
  EXPECT_EQ(url::Origin::Create(GURL(kExampleOrigin)), origins[0]);

  EXPECT_TRUE(example_file.IsValid());
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();
}

TEST_P(NativeIOManagerTest,
       GetOriginsByType_EmptyForUnimplementedStorageTypes) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_host_remote.BindNewPipeAndPassReceiver())
          .file;
  example_file.Close();
  NativeIOFileHostSync example_file_host(example_host_remote.get());
  example_file_host.Close();

  std::vector<url::Origin> origins =
      sync_manager_->GetOriginsForType(blink::mojom::StorageType::kPersistent);
  EXPECT_EQ(0u, origins.size());
  origins =
      sync_manager_->GetOriginsForType(blink::mojom::StorageType::kSyncable);
  EXPECT_EQ(0u, origins.size());
  origins = sync_manager_->GetOriginsForType(
      blink::mojom::StorageType::kQuotaNotManaged);
  EXPECT_EQ(0u, origins.size());
}

TEST_P(NativeIOManagerTest, GetOriginsByHost_ReturnsActiveOrigins) {
  mojo::Remote<blink::mojom::NativeIOFileHost> example_file_host_remote;
  base::File example_file =
      example_host_
          ->OpenFile("test_file",
                     example_file_host_remote.BindNewPipeAndPassReceiver())
          .file;

  mojo::Remote<blink::mojom::NativeIOHost> example_with_port_host_remote;
  std::string example_with_port_origin =
      std::string(kExampleOrigin).append(":1");
  manager_->BindReceiver(
      url::Origin::Create(GURL(example_with_port_origin)),
      example_with_port_host_remote.BindNewPipeAndPassReceiver(),
      GetBadMessageCallback());
  NativeIOHostSync example_with_port_host(example_with_port_host_remote.get());
  mojo::Remote<blink::mojom::NativeIOFileHost>
      example_with_port_file_host_remote;
  base::File example_with_port_file =
      example_with_port_host
          .OpenFile(
              "test_file",
              example_with_port_file_host_remote.BindNewPipeAndPassReceiver())
          .file;

  mojo::Remote<blink::mojom::NativeIOFileHost> google_file_host_remote;
  base::File google_file =
      google_host_
          ->OpenFile("test_file",
                     google_file_host_remote.BindNewPipeAndPassReceiver())
          .file;

  example_file.Close();
  NativeIOFileHostSync example_file_host(example_file_host_remote.get());
  example_file_host.Close();
  example_with_port_file.Close();
  NativeIOFileHostSync example_with_port_file_host(
      example_with_port_file_host_remote.get());
  example_with_port_file_host.Close();
  google_file.Close();
  NativeIOFileHostSync google_file_host(google_file_host_remote.get());
  google_file_host.Close();

  std::vector<url::Origin> example_origins = sync_manager_->GetOriginsForHost(
      blink::mojom::StorageType::kTemporary, "example.com");
  EXPECT_EQ(2u, example_origins.size());
  EXPECT_THAT(
      example_origins,
      testing::Contains(url::Origin::Create(GURL(example_with_port_origin))));
  EXPECT_THAT(example_origins,
              testing::Contains(url::Origin::Create(GURL(kExampleOrigin))));

  std::vector<url::Origin> google_origins = sync_manager_->GetOriginsForHost(
      blink::mojom::StorageType::kTemporary, "google.com");
  EXPECT_EQ(1u, google_origins.size());
  EXPECT_EQ(url::Origin::Create(GURL(kGoogleOrigin)), google_origins[0]);
}

TEST_P(NativeIOManagerTest, GetOriginUsage_ActiveOriginUsage) {
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

  int64_t usage =
      sync_manager_->GetOriginUsage(url::Origin::Create(GURL(kExampleOrigin)),
                                    blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(expected_usage, usage);
}

TEST_P(NativeIOManagerTest, GetOriginUsage_NonexistingOriginUsage) {
  int64_t usage =
      sync_manager_->GetOriginUsage(url::Origin::Create(GURL(kExampleOrigin)),
                                    blink::mojom::StorageType::kTemporary);

  EXPECT_EQ(0u, usage);
}

INSTANTIATE_TEST_CASE_P(,
                        NativeIOManagerTest,
                        ::testing::Values(
#if defined(OS_MAC)
                            false,
                            true
#else   // !defined(OS_MAC)
                            false
#endif  // defined(OS_MAC)
                            ));

}  // namespace

}  // namespace content
