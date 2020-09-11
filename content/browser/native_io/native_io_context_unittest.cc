// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "content/browser/native_io/native_io_context.h"
#include "content/test/fake_mojo_message_dispatch_context.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/test_support/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/native_io/native_io.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

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

  base::File OpenFile(
      const std::string& name,
      mojo::PendingReceiver<blink::mojom::NativeIOFileHost> file_receiver) {
    base::File file;
    base::RunLoop run_loop;
    io_host_->OpenFile(name, std::move(file_receiver),
                       base::BindLambdaForTesting([&](base::File backend_file) {
                         file = std::move(backend_file);
                         run_loop.Quit();
                       }));
    run_loop.Run();
    return file;
  }

  bool DeleteFile(const std::string& name) {
    bool success = false;
    base::RunLoop run_loop;
    io_host_->DeleteFile(name,
                         base::BindLambdaForTesting([&](bool backend_success) {
                           success = backend_success;
                           run_loop.Quit();
                         }));
    run_loop.Run();
    return success;
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

  bool RenameFile(const std::string& old_name, const std::string& new_name) {
    bool success = false;
    base::RunLoop run_loop;
    io_host_->RenameFile(old_name, new_name,
                         base::BindLambdaForTesting([&](bool backend_success) {
                           success = backend_success;
                           run_loop.Quit();
                         }));
    run_loop.Run();
    return success;
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

  std::pair<bool, base::File> SetLength(const int64_t length, base::File file) {
    bool success;
    base::RunLoop run_loop;
    file_host_->SetLength(
        length, std::move(file),
        base::BindLambdaForTesting(
            [&](bool backend_success, base::File backend_file) {
              success = backend_success;
              file = std::move(backend_file);
              run_loop.Quit();
            }));
    run_loop.Run();
    return {success, std::move(file)};
  }

 private:
  blink::mojom::NativeIOFileHost* const file_host_;
};

const char kExampleOrigin[] = "https://example.com";
const char kGoogleOrigin[] = "https://google.com";

class NativeIOContextTest : public testing::Test {
 public:
  void SetUp() override {
    ASSERT_TRUE(data_dir_.CreateUniqueTempDir());
    context_ = std::make_unique<NativeIOContext>(data_dir_.GetPath());

    context_->BindReceiver(url::Origin::Create(GURL(kExampleOrigin)),
                           example_host_remote_.BindNewPipeAndPassReceiver());
    context_->BindReceiver(url::Origin::Create(GURL(kGoogleOrigin)),
                           google_host_remote_.BindNewPipeAndPassReceiver());

    example_host_ = std::make_unique<NativeIOHostSync>(
        std::move(example_host_remote_.get()));
    google_host_ = std::make_unique<NativeIOHostSync>(
        std::move(google_host_remote_.get()));
  }

 protected:
  // This must be above NativeIOContext, to ensure that no file is accessed when
  // the temporary directory is deleted.
  base::ScopedTempDir data_dir_;

  // These tests need a full TaskEnvironment because NativeIOHost uses the
  // thread pool for file I/O.
  base::test::TaskEnvironment task_environment_;

  // The NativeIOContext is on the heap because it requires the profile path at
  // construction, and we only know the path during SetUp.
  std::unique_ptr<NativeIOContext> context_;

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
};

TEST_F(NativeIOContextTest, OpenFile_BadNames) {
  for (const std::string& bad_name : bad_names_) {
    mojo::test::BadMessageObserver bad_message_observer;

    mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
    base::File result = example_host_->OpenFile(
        bad_name, file_host.BindNewPipeAndPassReceiver());
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
  }
}

TEST_F(NativeIOContextTest, OpenFile_Locks_OpenFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(file.IsValid());

  mojo::Remote<blink::mojom::NativeIOFileHost> locked_file_host_remote;
  base::File locked_file = example_host_->OpenFile(
      "test_file", locked_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_FALSE(locked_file.IsValid());
}

TEST_F(NativeIOContextTest, OpenFile_SameName) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  base::File same_file = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            same_file.Read(0, read_buffer, kTestData.size()));
  EXPECT_EQ(kTestData, std::string(read_buffer, kTestData.size()));
}

TEST_F(NativeIOContextTest, DeleteFile_BadNames) {
  for (const std::string& bad_name : bad_names_) {
    mojo::test::BadMessageObserver bad_message_observer;

    EXPECT_FALSE(example_host_->DeleteFile(bad_name));
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
  }
}

TEST_F(NativeIOContextTest, OpenFile_Locks_DeleteFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  base::File file = example_host_->OpenFile(
      "test_file", file_host.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(file.IsValid());

  EXPECT_FALSE(example_host_->DeleteFile("test_file"));
}

TEST_F(NativeIOContextTest, OpenFile_Locks_RenameFile) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host;
  base::File file = example_host_->OpenFile(
      "test_file_in_use", file_host.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(file.IsValid());

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host2;
  base::File file2 = example_host_->OpenFile(
      "test_file_closed", file_host2.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(file2.IsValid());
  file2.Close();
  NativeIOFileHostSync file_host2_sync(file_host2.get());
  file_host2_sync.Close();

  EXPECT_FALSE(
      example_host_->RenameFile("test_file_in_use", "renamed_test_file"))
      << "An open file cannot be renamed";

  EXPECT_FALSE(
      example_host_->RenameFile("test_file_closed", "test_file_in_use"))
      << "An open file cannot be overwritten";
  ;
}

TEST_F(NativeIOContextTest, DeleteFile_WipesData) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(file.IsValid());
  EXPECT_EQ(static_cast<int>(kTestData.size()),
            file.Write(0, kTestData.data(), kTestData.size()));
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  EXPECT_TRUE(example_host_->DeleteFile("test_file"));

  mojo::Remote<blink::mojom::NativeIOFileHost> same_file_host_remote;
  base::File same_file = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(0, same_file.Read(0, read_buffer, kTestData.size()));
}

TEST_F(NativeIOContextTest, GetAllFiles_Empty) {
  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(0u, file_names.size());
}

TEST_F(NativeIOContextTest, GetAllFiles_AfterOpen) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());
  EXPECT_EQ("test_file", file_names[0]);
}

TEST_F(NativeIOContextTest, RenameFile_AfterOpenAndRename) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  example_host_->RenameFile("test_file", "renamed_test_file");
  std::vector<std::string> file_names = example_host_->GetAllFileNames();
  EXPECT_EQ(1u, file_names.size());
  EXPECT_EQ("renamed_test_file", file_names[0]);
}

TEST_F(NativeIOContextTest, RenameFile_BadNames) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  file.Close();
  NativeIOFileHostSync file_host(file_host_remote.get());
  file_host.Close();

  for (const std::string& bad_name : bad_names_) {
    mojo::test::BadMessageObserver bad_message_observer;

    EXPECT_FALSE(example_host_->RenameFile("test_file", bad_name));
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());

    EXPECT_FALSE(example_host_->RenameFile(bad_name, "inexistant_test_file"));
    EXPECT_EQ("Invalid file name", bad_message_observer.WaitForBadMessage());
  }
}

TEST_F(NativeIOContextTest, SetLength_NegativeLength) {
  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = example_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
  NativeIOFileHostSync file_host(file_host_remote.get());
  std::pair<bool, base::File> res = file_host.SetLength(-5, std::move(file));
  EXPECT_FALSE(res.first) << "A file can have no negative length.";

  res.second.Close();
  file_host.Close();
}

TEST_F(NativeIOContextTest, OriginIsolation) {
  const std::string kTestData("Test Data");

  mojo::Remote<blink::mojom::NativeIOFileHost> file_host_remote;
  base::File file = google_host_->OpenFile(
      "test_file", file_host_remote.BindNewPipeAndPassReceiver());
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
  base::File same_file = example_host_->OpenFile(
      "test_file", same_file_host_remote.BindNewPipeAndPassReceiver());
  EXPECT_TRUE(same_file.IsValid());
  char read_buffer[kTestData.size()];
  EXPECT_EQ(0, same_file.Read(0, read_buffer, kTestData.size()));
}

TEST_F(NativeIOContextTest, BindReceiver_UntrustworthyOrigin) {
  mojo::Remote<blink::mojom::NativeIOHost> insecure_host_remote_;

  // Create a fake dispatch context to trigger a bad message in.
  FakeMojoMessageDispatchContext fake_dispatch_context;
  mojo::test::BadMessageObserver bad_message_observer;
  context_->BindReceiver(url::Origin::Create(GURL("http://insecure.com")),
                         insecure_host_remote_.BindNewPipeAndPassReceiver());
  EXPECT_EQ("Called NativeIO from an insecure context",
            bad_message_observer.WaitForBadMessage());
}

}  // namespace

}  // namespace content
