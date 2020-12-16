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
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

using blink::mojom::NativeIOErrorPtr;
using blink::mojom::NativeIOErrorType;

namespace content {

namespace {

// Synchronous proxies to a wrapped NativeIOHost's methods.
class NativeIOHostSync {
 public:
  // The caller must ensure that the NativeIOHost outlives this.
  explicit NativeIOHostSync(blink::mojom::NativeIOHost* io_host)
      : io_host_(io_host) {}

  NativeIOHostSync(const NativeIOHostSync&) = delete;
  NativeIOHostSync operator=(const NativeIOHostSync&) = delete;

  ~NativeIOHostSync() = default;

  std::pair<base::File, NativeIOErrorPtr> OpenFile(
      const std::string& name,
      mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_receiver) {
    base::File file;
    NativeIOErrorPtr error;
    base::RunLoop run_loop;
    io_host_->OpenFile(
        name, std::move(file_receiver),
        base::BindLambdaForTesting(
            [&](base::File backend_file, NativeIOErrorPtr open_error) {
              file = std::move(backend_file);
              error = std::move(open_error);
              run_loop.Quit();
            }));
    run_loop.Run();
    return {std::move(file), std::move(error)};
  }

  NativeIOErrorPtr DeleteFile(const std::string& name) {
    NativeIOErrorPtr error;
    base::RunLoop run_loop;
    io_host_->DeleteFile(
        name, base::BindLambdaForTesting([&](NativeIOErrorPtr delete_error) {
          error = std::move(delete_error);
          run_loop.Quit();
        }));
    run_loop.Run();
    return error;
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
  NativeIOFileHostSync operator=(const NativeIOFileHostSync&) = delete;

  ~NativeIOFileHostSync() = default;

  void Close() {
    base::RunLoop run_loop;
    file_host_->Close(run_loop.QuitClosure());
    run_loop.Run();
    return;
  }

  std::pair<base::File, NativeIOErrorPtr> SetLength(const int64_t length,
                                                    base::File file) {
    NativeIOErrorPtr error;
    base::File returned_file;
    base::RunLoop run_loop;
    file_host_->SetLength(
        length, std::move(file),
        base::BindLambdaForTesting(
            [&](base::File backend_file, NativeIOErrorPtr set_length_error) {
              returned_file = std::move(backend_file);
              error = std::move(set_length_error);
              run_loop.Quit();
            }));
    run_loop.Run();
    return {std::move(returned_file), std::move(error)};
  }

 private:
  blink::mojom::NativeIOFileHost* const file_host_;
};

const char kExampleOrigin[] = "https://example.com";
const char kGoogleOrigin[] = "https://google.com";

class NativeIOManagerTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        /*is_incognito=*/false, data_dir_.GetPath(),
        base::ThreadTaskRunnerHandle::Get().get(),
        /*special storage policy=*/nullptr);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager(), base::ThreadTaskRunnerHandle::Get().get());
    manager_ = std::make_unique<NativeIOManager>(
        data_dir_.GetPath(),
        /*special storage policy=*/nullptr, quota_manager_proxy());

    manager_->BindReceiver(url::Origin::Create(GURL(kExampleOrigin)),
                           example_host_remote_.BindNewPipeAndPassReceiver());
    manager_->BindReceiver(url::Origin::Create(GURL(kGoogleOrigin)),
                           google_host_remote_.BindNewPipeAndPassReceiver());

    example_host_ = std::make_unique<NativeIOHostSync>(
        std::move(example_host_remote_.get()));
    google_host_ = std::make_unique<NativeIOHostSync>(
        std::move(google_host_remote_.get()));
  }

  void TearDown() override {
    // Let the client go away before dropping a ref of the quota manager proxy.
    quota_manager_proxy()->SimulateQuotaManagerDestroyed();
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

  std::string GetTooLongFilename() {
    int limit = base::GetMaximumPathComponentLength(data_dir_.GetPath());
    EXPECT_GT(limit, 0);

    std::string too_long_filename(limit + 1, 'x');
    return too_long_filename;
  }

  // This must be above NativeIOManager, to ensure that no file is accessed when
  // the temporary directory is deleted.
  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because NativeIOHost uses the
  // thread pool for file I/O.
  base::test::TaskEnvironment task_environment_;

  // The NativeIOManager is on the heap because it requires the profile path at
  // construction, and we only know the path during SetUp.
  std::unique_ptr<NativeIOManager> manager_;

  // Hosts for two different origins, used for isolation testing.
  mojo::Remote<blink::mojom::NativeIOHost> example_host_remote_;
  mojo::Remote<blink::mojom::NativeIOHost> google_host_remote_;
  std::unique_ptr<NativeIOHostSync> example_host_;
  std::unique_ptr<NativeIOHostSync> google_host_;

  // Names disallowed by NativeIO
  const std::vector<std::string> bad_names_ = {
      "Uppercase",
      "has-dash",
      "has.dot",
      "has/slash",
  };

 private:
  scoped_refptr<storage::QuotaManager> quota_manager_;
  scoped_refptr<storage::QuotaManagerProxy> quota_manager_proxy_;
};

TEST_F(NativeIOManagerTest, OpenFile_BadNames) {
  for (const std::string& bad_name : bad_names_) {
    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
    std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
        bad_name, file_host.BindNewPipeAndPassReceiver());
    EXPECT_FALSE(result.first.IsValid());
    EXPECT_EQ(result.second->type, NativeIOErrorType::kUnknown);
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
  }
  // TODO(rstz): Have the renderer process disallow too long filenames and then
  // re-enable testing for long filenames on Windows.
#if !defined(OS_WIN)
  std::string too_long_filename = GetTooLongFilename();
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      too_long_filename, file_host.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(result.first.IsValid());
  EXPECT_EQ(result.second->type, NativeIOErrorType::kInvalidState);
#endif  // !defined(OS_WIN)
}

TEST_F(NativeIOManagerTest, OpenFile_Locks_OpenFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(result.first.IsValid());

  mojo::Remote<blink::mojom::NativeIOFileHost> locked_file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> locked_result =
      example_host_->OpenFile(
          "test_file", locked_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(locked_result.first.IsValid());
  EXPECT_EQ(locked_result.second->type,
            NativeIOErrorType::kNoModificationAllowed)
      << "A file cannot be opened twice";
}

TEST_F(NativeIOManagerTest, OpenFile_SameName) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  base::File& file = result.first;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> same_result = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  base::File& same_file = same_result.first;
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            same_file.Read(0, read_buffer, kTestData.size()));
  EXPECT_EQ(kTestData, std::string(read_buffer, kTestData.size()));
}

// TODO(rstz): Consider failing upon deletion of an overly long file name for
// consistency with rename and open.
TEST_F(NativeIOManagerTest, DeleteFile_BadNames) {
  for (const std::string& bad_name : bad_names_) {
    mojo::test::BadMessageObserver bad_message_observer;

    EXPECT_EQ(example_host_->DeleteFile(bad_name)->type,
              NativeIOErrorType::kUnknown);
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
  }
}

TEST_F(NativeIOManagerTest, OpenFile_Locks_DeleteFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(result.first.IsValid());

  EXPECT_EQ(example_host_->DeleteFile("test_file")->type,
            NativeIOErrorType::kNoModificationAllowed);
}

TEST_F(NativeIOManagerTest, OpenFile_Locks_RenameFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file_in_use", file_host.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(result.first.IsValid());

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host2;
  std::pair<base::File, NativeIOErrorPtr> result_closed =
      example_host_->OpenFile("test_file_closed",
                              file_host2.BindNewPipeAndPassReceiver());
  base::File& file_closed = result_closed.first;
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

TEST_F(NativeIOManagerTest, DeleteFile_WipesData) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  base::File& file = result.first;
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  EXPECT_EQ(example_host_->DeleteFile("test_file")->type,
            NativeIOErrorType::kSuccess);

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> same_result = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  base::File& same_file = same_result.first;
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(0, same_file.Read(0, read_buffer, kTestData.size()));
}

TEST_F(NativeIOManagerTest, GetAllFiles_Empty) {
  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(0u, file_names.size());
}

TEST_F(NativeIOManagerTest, GetAllFiles_AfterOpen) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  result.first.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());
  EXPECT_EQ("test_file", file_names[0]);
}

TEST_F(NativeIOManagerTest, RenameFile_AfterOpenAndRename) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  result.first.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  example_host_->RenameFile("test_file", "renamed_test_file");
  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());
  EXPECT_EQ("renamed_test_file", file_names[0]);
}

TEST_F(NativeIOManagerTest, RenameFile_BadNames) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  result.first.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  for (const std::string& bad_name : bad_names_) {
    mojo::test::BadMessageObserver bad_message_observer;

    EXPECT_EQ(example_host_->RenameFile("test_file", bad_name)->type,
              NativeIOErrorType::kUnknown);
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());

    EXPECT_EQ(example_host_->RenameFile(bad_name, "inexistant_test_file")->type,
              NativeIOErrorType::kUnknown);
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
  }
  // TODO(rstz): Have the renderer process disallow too long filenames and then
  // re-enable testing for long filenames on Windows.
#if !defined(OS_WIN)
  std::string too_long_filename = GetTooLongFilename();
  EXPECT_EQ(example_host_->RenameFile("test_file", too_long_filename)->type,
            NativeIOErrorType::kInvalidState);
#endif  // !defined(OS_WIN)
}

TEST_F(NativeIOManagerTest, SetLength_NegativeLength) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  base::File& file = result.first;
  NativeIOFileHostSync file_host(file_host_remote.get());
  std::pair<base::File, NativeIOErrorPtr> res =
      file_host.SetLength(-5, std::move(file));
  EXPECT_EQ(res.second->type, NativeIOErrorType::kUnknown)
      << "The file length cannot be negative.";

  res.first.Close();
  file_host.Close();
}

TEST_F(NativeIOManagerTest, OriginIsolation) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  std::pair<base::File, NativeIOErrorPtr> result = google_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  base::File& file = result.first;
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
  std::pair<base::File, NativeIOErrorPtr> same_result = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  base::File& same_file = same_result.first;
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(0, same_file.Read(0, read_buffer, kTestData.size()));
}

TEST_F(NativeIOManagerTest, BindReceiver_UntrustworthyOrigin) {
  mojo::Remote<blink::mojom::NativeIOHost> insecure_host_remote_;

  // Create a fake dispatch context to trigger a bad message in.
  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  manager_->BindReceiver(url::Origin::Create(GURL("http://insecure.com")),
                         insecure_host_remote_.BindNewPipeAndPassReceiver());
  EXPECT_EQ("Called NativeIO from an insecure context",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace

}  // namespace content
