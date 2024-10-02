// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system/file_system_url_loader_factory.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/containers/heap_array.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/i18n/unicodestring.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/test/mock_scoped_file_access_delegate.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/mime_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_util.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/file_system/external_mount_points.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_file_util.h"
#include "storage/browser/file_system/file_system_operation_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/sandbox_file_system_backend_delegate.h"
#include "storage/browser/quota/quota_manager_proxy.h"
#include "storage/browser/test/async_file_test_helper.h"
#include "storage/browser/test/mock_quota_manager.h"
#include "storage/browser/test/mock_quota_manager_proxy.h"
#include "storage/browser/test/mock_special_storage_policy.h"
#include "storage/browser/test/test_file_system_backend.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/browser/test/test_file_system_options.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/regex.h"
#include "url/gurl.h"
#include "url/origin.h"

using base::BindLambdaForTesting;
using network::mojom::URLLoaderFactory;
using storage::AsyncFileTestHelper;
using storage::FileSystemContext;
using storage::FileSystemOperationContext;
using storage::FileSystemURL;

namespace content {
namespace {

enum class TestMode { kRegular, kIncognito };

// We always use the TEMPORARY FileSystem in these tests.
constexpr char kFileSystemURLPrefix[] = "filesystem:http://remote/temporary/";

constexpr char kValidExternalMountPoint[] = "mnt_name";

constexpr char kTestFileData[] = "0123456789";

void FillBuffer(base::span<uint8_t> buffer) {
  base::RandBytes(buffer);
}

// An auto mounter that will try to mount anything for `storage_domain` =
// "automount", but will only succeed for the mount point "mnt_name".
bool TestAutoMountForURLRequest(
    const storage::FileSystemRequestInfo& request_info,
    const storage::FileSystemURL& filesystem_url,
    base::OnceCallback<void(base::File::Error result)> callback) {
  if (request_info.storage_domain != "automount")
    return false;

  std::vector<base::FilePath::StringType> components =
      filesystem_url.path().GetComponents();
  std::string mount_point = base::FilePath(components[0]).AsUTF8Unsafe();

  if (mount_point == kValidExternalMountPoint) {
    storage::ExternalMountPoints::GetSystemInstance()->RegisterFileSystem(
        kValidExternalMountPoint, storage::kFileSystemTypeTest,
        storage::FileSystemMountOption(), base::FilePath());
    std::move(callback).Run(base::File::FILE_OK);
  } else {
    std::move(callback).Run(base::File::FILE_ERROR_NOT_FOUND);
  }
  return true;
}

void ReadDataPipeInternal(mojo::DataPipeConsumerHandle handle,
                          std::string* result,
                          base::OnceClosure quit_closure) {
  while (true) {
    base::span<const uint8_t> buffer;
    MojoResult rv = handle.BeginReadData(MOJO_READ_DATA_FLAG_NONE, buffer);
    switch (rv) {
      case MOJO_RESULT_BUSY:
      case MOJO_RESULT_INVALID_ARGUMENT:
        NOTREACHED_IN_MIGRATION();
        return;
      case MOJO_RESULT_FAILED_PRECONDITION:
        std::move(quit_closure).Run();
        return;
      case MOJO_RESULT_SHOULD_WAIT:
        base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
            FROM_HERE, base::BindOnce(&ReadDataPipeInternal, handle, result,
                                      std::move(quit_closure)));
        return;
      case MOJO_RESULT_OK:
        EXPECT_NE(nullptr, buffer.data());
        EXPECT_GT(buffer.size(), 0u);
        size_t before_size = result->size();
        result->append(base::as_string_view(buffer));
        size_t read_size = result->size() - before_size;
        EXPECT_EQ(buffer.size(), read_size);
        rv = handle.EndReadData(read_size);
        EXPECT_EQ(MOJO_RESULT_OK, rv);
        break;
    }
  }
  NOTREACHED_IN_MIGRATION();
  return;
}

std::string ReadDataPipe(mojo::ScopedDataPipeConsumerHandle handle) {
  EXPECT_TRUE(handle.is_valid());
  if (!handle.is_valid())
    return "";
  std::string result;
  base::RunLoop loop;
  ReadDataPipeInternal(handle.get(), &result, loop.QuitClosure());
  loop.Run();
  return result;
}

// Directory listings can have a HTML header in the file to format the response.
// This function determines if a single line in the response is for a directory
// entry.
bool IsDirectoryListingLine(const std::string& line) {
  return line.find("<script>addRow(\"") == 0;
}

// Is the line a title inserted by net::GetDirectoryListingHeader?
bool IsDirectoryListingTitle(const std::string& line) {
  return line.find("<script>start(\"") == 0;
}

}  // namespace

class FileSystemURLLoaderFactoryTest
    : public ContentBrowserTest,
      public ::testing::WithParamInterface<TestMode> {
 public:
  FileSystemURLLoaderFactoryTest(const FileSystemURLLoaderFactoryTest&) =
      delete;
  FileSystemURLLoaderFactoryTest& operator=(
      const FileSystemURLLoaderFactoryTest&) = delete;

 protected:
  FileSystemURLLoaderFactoryTest() = default;
  ~FileSystemURLLoaderFactoryTest() override = default;

  bool IsIncognito() { return GetParam() == TestMode::kIncognito; }

  void SetUpOnMainThread() override {
    io_task_runner_ = GetIOThreadTaskRunner({});
    blocking_task_runner_ =
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()});

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    special_storage_policy_ =
        base::MakeRefCounted<storage::MockSpecialStoragePolicy>();
    quota_manager_ = base::MakeRefCounted<storage::MockQuotaManager>(
        IsIncognito(), temp_dir_.GetPath(), io_task_runner_,
        special_storage_policy_);
    quota_manager_proxy_ = base::MakeRefCounted<storage::MockQuotaManagerProxy>(
        quota_manager_.get(), io_task_runner_);

    // Support multiple sites on the test server.
    host_resolver()->AddRule("*", "127.0.0.1");

    ContentBrowserTest::SetUpOnMainThread();

    // We use a test FileSystemContext which runs on the main thread, so we
    // can work with it synchronously.
    file_system_context_ = CreateFileSystemContext(temp_dir_.GetPath());
    file_util_ = file_system_context_->sandbox_delegate()->sync_file_util();

    // The filesystem must be opened on the IO sequence.
    base::RunLoop loop;
    io_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &FileSystemContext::OpenFileSystem, file_system_context_,
            blink::StorageKey::CreateFromStringForTesting("http://remote/"),
            /*bucket=*/std::nullopt, storage::kFileSystemTypeTemporary,
            storage::OPEN_FILE_SYSTEM_CREATE_IF_NONEXISTENT,
            base::BindOnce(&FileSystemURLLoaderFactoryTest::OnOpenFileSystem,
                           loop.QuitClosure())));
    loop.Run();
  }

  void TearDownOnMainThread() override {
    loader_.reset();
    base::RunLoop loop;
    io_task_runner_->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                                if (file_system_context_) {
                                  file_system_context_->Shutdown();
                                  file_system_context_ = nullptr;
                                }
                                loop.Quit();
                              }));
    loop.Run();
    special_storage_policy_ = nullptr;
    // FileReader posts a task to close the file in destructor.
    base::RunLoop().RunUntilIdle();
    ContentBrowserTest::TearDownOnMainThread();
  }

  base::FilePath SetUpAutoMountContext() {
    base::FilePath mnt_point =
        temp_dir_.GetPath().AppendASCII("auto_mount_dir");
    EXPECT_TRUE(base::CreateDirectory(mnt_point));

    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(
        std::make_unique<storage::TestFileSystemBackend>(
            base::SingleThreadTaskRunner::GetCurrentDefault().get(),
            mnt_point));

    std::vector<storage::URLRequestAutoMountHandler> handlers = {
        base::BindRepeating(&TestAutoMountForURLRequest)};

    file_system_context_ = CreateFileSystemContextWithAutoMountersForTesting(
        io_task_runner_, blocking_task_runner_, nullptr,
        std::move(additional_providers), handlers, temp_dir_.GetPath());
    return mnt_point;
  }

  void SetUpFileAutoMountContext() {
    const base::FilePath mnt_point = SetUpAutoMountContext();

    ASSERT_TRUE(base::WriteFile(mnt_point.AppendASCII("foo"), kTestFileData));
  }

  FileSystemURL CreateURL(const base::FilePath& file_path) {
    return file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting("http://remote"),
        storage::kFileSystemTypeTemporary, file_path);
  }

  void CreateDirectory(const std::string_view& dir_name) {
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());
    base::RunLoop loop;
    blocking_task_runner_->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          base::FilePath path = base::FilePath().AppendASCII(dir_name);
          ASSERT_EQ(base::File::FILE_OK,
                    file_util_->CreateDirectory(context.get(), CreateURL(path),
                                                /*exclusive=*/false,
                                                /*recursive=*/false));
          loop.Quit();
        }));
    loop.Run();
  }

  void WriteFile(const std::string_view& file_name,
                 base::span<const uint8_t> buf) {
    FileSystemURL url;
    url = file_system_context_->CreateCrackedFileSystemURL(
        blink::StorageKey::CreateFromStringForTesting("http://remote"),
        storage::kFileSystemTypeTemporary,
        base::FilePath().AppendASCII(file_name));

    base::File::Error result = base::File::FILE_OK;
    base::FilePath local_path;
    base::ScopedTempDir dir;
    if (!dir.CreateUniqueTempDir()) {
      result = base::File::FILE_ERROR_FAILED;
    }
    local_path = dir.GetPath().AppendASCII("tmp");
    if (!base::WriteFile(local_path, buf)) {
      result = base::File::FILE_ERROR_FAILED;
    }
    EXPECT_EQ(base::File::FILE_OK, result);

    base::RunLoop loop;
    io_task_runner_->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          file_system_context_->operation_runner()->CopyInForeignFile(
              local_path, url, BindLambdaForTesting([&](base::File::Error err) {
                result = err;
                loop.Quit();
              }));
        }));
    loop.Run();
    EXPECT_EQ(base::File::FILE_OK, result);
  }

  void EnsureFileExists(std::string_view file_name) {
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());

    base::RunLoop loop;
    blocking_task_runner_->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          base::FilePath path = base::FilePath().AppendASCII(file_name);
          ASSERT_EQ(base::File::FILE_OK,
                    file_util_->EnsureFileExists(context.get(), CreateURL(path),
                                                 nullptr));
          loop.Quit();
        }));
    loop.Run();
  }

  void TruncateFile(std::string_view file_name, int64_t length) {
    std::unique_ptr<FileSystemOperationContext> context(NewOperationContext());

    base::RunLoop loop;
    blocking_task_runner_->PostTask(
        FROM_HERE, BindLambdaForTesting([&]() {
          base::FilePath path = base::FilePath().AppendASCII(file_name);
          ASSERT_EQ(
              base::File::FILE_OK,
              file_util_->Truncate(context.get(), CreateURL(path), length));
          loop.Quit();
        }));
    loop.Run();
  }

  // If `size` is negative, the reported size is ignored.
  void VerifyListingEntry(const std::string& entry_line,
                          const std::string& name,
                          const std::string& url,
                          bool is_directory,
                          int64_t size) {
#define NUMBER "([0-9-]*)"
#define STR "([^\"]*)"
    icu::UnicodeString pattern("^<script>addRow\\(\"" STR "\",\"" STR
                               "\",(0|1)," NUMBER ",\"" STR "\"," NUMBER
                               ",\"" STR "\"\\);</script>");
#undef NUMBER
#undef STR
    icu::UnicodeString input(entry_line.c_str());

    UErrorCode status = U_ZERO_ERROR;
    icu::RegexMatcher match(pattern, input, 0, status);

    EXPECT_TRUE(match.find());
    EXPECT_EQ(7, match.groupCount());
    EXPECT_EQ(icu::UnicodeString(name.c_str()), match.group(1, status));
    EXPECT_EQ(icu::UnicodeString(url.c_str()), match.group(2, status));
    EXPECT_EQ(icu::UnicodeString(is_directory ? "1" : "0"),
              match.group(3, status));
    if (size >= 0) {
      icu::UnicodeString size_string(
          base::FormatBytesUnlocalized(size).c_str());
      EXPECT_EQ(size_string, match.group(5, status));
    }

    icu::UnicodeString date_ustr(match.group(7, status));
    std::unique_ptr<icu::DateFormat> formatter(
        icu::DateFormat::createDateTimeInstance(icu::DateFormat::kShort));
    UErrorCode parse_status = U_ZERO_ERROR;
    UDate udate = formatter->parse(date_ustr, parse_status);
    EXPECT_TRUE(U_SUCCESS(parse_status));
    base::Time date = base::Time::FromMillisecondsSinceUnixEpoch(udate);
    EXPECT_FALSE(date.is_null());
  }

  GURL CreateFileSystemURL(const std::string& path) {
    return GURL(kFileSystemURLPrefix + path);
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoad(const GURL& url) {
    auto client =
        TestLoadHelper(url, /*origin=*/std::nullopt, /*extra_headers=*/nullptr,
                       file_system_context_);
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadWithInitiator(
      const GURL& url,
      const url::Origin& origin) {
    auto client = TestLoadHelper(url, origin, /*extra_headers=*/nullptr,
                                 file_system_context_);
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadWithContext(
      const GURL& url,
      scoped_refptr<storage::FileSystemContext> file_system_context) {
    auto client =
        TestLoadHelper(url, /*origin=*/std::nullopt, /*extra_headers=*/nullptr,
                       file_system_context);
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadWithHeaders(
      const GURL& url,
      const net::HttpRequestHeaders* extra_headers) {
    auto client = TestLoadHelper(url, /*origin=*/std::nullopt, extra_headers,
                                 file_system_context_);
    client->RunUntilComplete();
    return client;
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadNoRun(
      const GURL& url,
      const net::HttpRequestHeaders* extra_headers = nullptr) {
    return TestLoadHelper(url, /*origin=*/std::nullopt, extra_headers,
                          file_system_context_);
  }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner() {
    return io_task_runner_;
  }

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner() {
    return blocking_task_runner_;
  }

  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy() {
    return quota_manager_proxy_;
  }

  // `temp_dir_` must be deleted last.
  base::ScopedTempDir temp_dir_;
  mojo::Remote<network::mojom::URLLoader> loader_;

 private:
  static void OnOpenFileSystem(base::OnceClosure done_closure,
                               const FileSystemURL& root_url,
                               const std::string& name,
                               base::File::Error result) {
    ASSERT_EQ(base::File::FILE_OK, result);
    std::move(done_closure).Run();
  }

  scoped_refptr<storage::FileSystemContext> CreateFileSystemContext(
      const base::FilePath& base_path) {
    std::vector<std::unique_ptr<storage::FileSystemBackend>>
        additional_providers;
    additional_providers.push_back(
        std::make_unique<storage::TestFileSystemBackend>(
            blocking_task_runner_.get(), base_path));
    if (IsIncognito()) {
      return CreateIncognitoFileSystemContextWithAdditionalProvidersForTesting(
          io_task_runner_, blocking_task_runner_, quota_manager_proxy(),
          std::move(additional_providers), base_path);
    } else {
      return CreateFileSystemContextWithAdditionalProvidersForTesting(
          io_task_runner_, blocking_task_runner_, quota_manager_proxy(),
          std::move(additional_providers), base_path);
    }
  }

  std::unique_ptr<FileSystemOperationContext> NewOperationContext() {
    auto context = std::make_unique<FileSystemOperationContext>(
        file_system_context_.get());
    context->set_allowed_bytes_growth(1024);
    return context;
  }

  RenderFrameHost* render_frame_host() const {
    return shell()->web_contents()->GetPrimaryMainFrame();
  }

  std::unique_ptr<network::TestURLLoaderClient> TestLoadHelper(
      const GURL& url,
      const std::optional<url::Origin>& origin,
      const net::HttpRequestHeaders* extra_headers,
      scoped_refptr<storage::FileSystemContext> file_system_context) {
    network::ResourceRequest request;
    request.url = url;
    if (origin) {
      request.request_initiator = origin;
    }
    if (extra_headers)
      request.headers.MergeFrom(*extra_headers);
    const std::string storage_domain = url.DeprecatedGetOriginAsURL().host();
    mojo::Remote<network::mojom::URLLoaderFactory> factory(
        CreateFileSystemURLLoaderFactory(
            render_frame_host()->GetProcess()->GetID(),
            render_frame_host()->GetFrameTreeNodeId(), file_system_context,
            storage_domain,
            blink::StorageKey::CreateFirstParty(url::Origin::Create(url))));

    auto client = std::make_unique<network::TestURLLoaderClient>();
    loader_.reset();
    factory->CreateLoaderAndStart(
        loader_.BindNewPipeAndPassReceiver(), 0,
        network::mojom::kURLLoadOptionNone, request, client->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

    return client;
  }

  scoped_refptr<storage::MockSpecialStoragePolicy> special_storage_policy_;
  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_refptr<FileSystemContext> file_system_context_;
  scoped_refptr<storage::MockQuotaManager> quota_manager_;
  scoped_refptr<storage::MockQuotaManagerProxy> quota_manager_proxy_;
  // Owned by `file_system_context_` and only usable on `blocking_task_runner_`.
  raw_ptr<storage::FileSystemFileUtil, AcrossTasksDanglingUntriaged>
      file_util_ = nullptr;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FileSystemURLLoaderFactoryTest,
                         testing::Values(TestMode::kRegular,
                                         TestMode::kIncognito));

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, DirectoryListing) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("foo");
  CreateDirectory("foo/bar");
  CreateDirectory("foo/bar/baz");

  EnsureFileExists("foo/bar/hoge");
  TruncateFile("foo/bar/hoge", 10);

  auto client = TestLoad(CreateFileSystemURL("foo/bar/"));

  ASSERT_TRUE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());

  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_GT(response_text.size(), 0ul);

  std::istringstream in(response_text);
  std::string line;

  std::string listing_header;
  std::vector<std::string> listing_entries;
  while (!!std::getline(in, line)) {
    if (listing_header.empty() && IsDirectoryListingTitle(line)) {
      listing_header = line;
      continue;
    }
    if (IsDirectoryListingLine(line))
      listing_entries.push_back(line);
  }

#if BUILDFLAG(IS_WIN)
  EXPECT_EQ("<script>start(\"foo\\\\bar\");</script>", listing_header);
#elif BUILDFLAG(IS_POSIX)
  EXPECT_EQ("<script>start(\"/foo/bar\");</script>", listing_header);
#endif

  ASSERT_EQ(2U, listing_entries.size());
  std::sort(listing_entries.begin(), listing_entries.end());
  VerifyListingEntry(listing_entries[0], "baz", "baz", true, 0);
  VerifyListingEntry(listing_entries[1], "hoge", "hoge", false, 10);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, InvalidURL) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(GURL("filesystem:/foo/bar/baz"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_URL, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, NoSuchRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(GURL("filesystem:http://remote/persistent/somedir/"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, NoSuchDirectory) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(CreateFileSystemURL("somedir/"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, Cancel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("foo");
  auto client = TestLoadNoRun(CreateFileSystemURL("foo/"));
  ASSERT_FALSE(client->has_received_response());
  ASSERT_FALSE(client->has_received_completion());

  client.reset();
  loader_.reset();
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, Incognito) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("foo");

  scoped_refptr<FileSystemContext> file_system_context =
      storage::CreateIncognitoFileSystemContextForTesting(
          io_task_runner(), blocking_task_runner(), nullptr,
          temp_dir_.GetPath());

  auto client =
      TestLoadWithContext(CreateFileSystemURL("/"), file_system_context.get());
  // The request fails as the requested directory does not exist in in-memory
  // obfuscated file system.
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  client = TestLoadWithContext(CreateFileSystemURL("foo"),
                               file_system_context.get());
  ASSERT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest,
                       AutoMountDirectoryListing) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mnt_point = SetUpAutoMountContext();
  EXPECT_TRUE(base::CreateDirectory(mnt_point));
  EXPECT_TRUE(base::CreateDirectory(mnt_point.AppendASCII("foo")));
  EXPECT_TRUE(base::WriteFile(mnt_point.AppendASCII("bar"), "1234567890"));

  auto client =
      TestLoad(GURL("filesystem:http://automount/external/mnt_name/"));

  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());

  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_GT(response_text.size(), 0ul);

  std::istringstream in(response_text);
  std::string line;
  EXPECT_TRUE(std::getline(in, line));  // `line` contains the temp dir path.

  // Result order is not guaranteed, so sort the results.
  std::vector<std::string> listing_entries;
  while (!!std::getline(in, line)) {
    if (IsDirectoryListingLine(line))
      listing_entries.push_back(line);
  }

  ASSERT_EQ(2U, listing_entries.size());
  std::sort(listing_entries.begin(), listing_entries.end());
  VerifyListingEntry(listing_entries[0], "bar", "bar", false, 10);
  VerifyListingEntry(listing_entries[1], "foo", "foo", true, -1);

  EXPECT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, AutoMountInvalidRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mnt_point = SetUpAutoMountContext();
  auto client = TestLoad(GURL("filesystem:http://automount/external/invalid"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  EXPECT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          "invalid"));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, AutoMountNoHandler) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath mnt_point = SetUpAutoMountContext();
  auto client = TestLoad(GURL("filesystem:http://noauto/external/mnt_name"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  EXPECT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile(
      "file1.dat",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));
  auto client = TestLoad(CreateFileSystemURL("file1.dat"));

  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  ASSERT_TRUE(client->response_head()->headers) << "No response headers";
  EXPECT_EQ(200, client->response_head()->headers->response_code());
  std::string cache_control;
  EXPECT_TRUE(client->response_head()->headers->GetNormalizedHeader(
      "cache-control", &cache_control));
  EXPECT_EQ("no-cache", cache_control);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileTestDlp) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  base::MockRepeatingCallback<void(
      const std::vector<base::FilePath>&,
      base::OnceCallback<void(file_access::ScopedFileAccess)>)>
      fileAccessCallback;
  file_access::MockScopedFileAccessDelegate scoped_file_access_delegate;
  EXPECT_CALL(scoped_file_access_delegate, CreateFileAccessCallback)
      .WillOnce(::testing::Return(fileAccessCallback.Get()));

  WriteFile(
      "file1.dat",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));
  auto client =
      TestLoadWithInitiator(CreateFileSystemURL("file1.dat"),
                            url::Origin::Create(GURL("https://example.com")));

  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  ASSERT_TRUE(client->response_head()->headers) << "No response headers";
  EXPECT_EQ(200, client->response_head()->headers->response_code());
  std::string cache_control;
  EXPECT_TRUE(client->response_head()->headers->GetNormalizedHeader(
      "cache-control", &cache_control));
  EXPECT_EQ("no-cache", cache_control);
}

// Verify that when site isolation is enabled, a renderer process for one
// origin can't request filesystem: URLs belonging to another origin.  See
// https://crbug.com/964245.
IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, CrossOriginFileBlocked) {
  IsolateAllSitesForTesting(base::CommandLine::ForCurrentProcess());

  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile(
      "file1.dat",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));

  // Navigate main frame to foo.com.
  ASSERT_TRUE(embedded_test_server()->Start());
  EXPECT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL("foo.com", "/title1.html")));

  // Try requesting filesystem:http://remote/temporary/file1.dat from that
  // frame.  This should be blocked, as foo.com isn't allowed to request a
  // filesystem URL for the http://remote origin.
  auto client = TestLoad(CreateFileSystemURL("file1.dat"));
  EXPECT_FALSE(client->has_received_response());
  ASSERT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_INVALID_URL, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest,
                       FileTestFullSpecifiedRange) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  // Size should be larger than kDefaultFileSystemUrlPipeSize to test
  // that large files are properly read in chunks and not entirely into
  // memory.
  const size_t buffer_size = 180'000;
  auto buffer = base::HeapArray<uint8_t>::Uninit(buffer_size);
  FillBuffer(buffer);
  WriteFile("bigfile", buffer);

  const size_t first_byte_position = 500;
  const size_t last_byte_position = buffer_size - first_byte_position;
  base::span<uint8_t> partial_buffer = buffer.subspan(
      first_byte_position, last_byte_position - first_byte_position + 1);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::Bounded(first_byte_position, last_byte_position)
          .GetHeaderValue());
  auto client = TestLoadNoRun(CreateFileSystemURL("bigfile"), &headers);
  client->RunUntilResponseBodyArrived();
  ASSERT_TRUE(client->has_received_response());
  std::string response_text = ReadDataPipe(client->response_body_release());
  client->RunUntilComplete();
  EXPECT_TRUE(client->has_received_completion());
  // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
  EXPECT_TRUE(partial_buffer == base::as_byte_span(response_text));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest,
                       FileTestHalfSpecifiedRange) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const size_t buffer_size = 4000;
  auto buffer = base::HeapArray<uint8_t>::Uninit(buffer_size);
  FillBuffer(buffer);
  WriteFile("bigfile", buffer);

  const size_t first_byte_position = 500;
  base::span<uint8_t> partial_buffer =
      buffer.subspan(first_byte_position, buffer_size - first_byte_position);

  net::HttpRequestHeaders headers;
  headers.SetHeader(
      net::HttpRequestHeaders::kRange,
      net::HttpByteRange::RightUnbounded(first_byte_position).GetHeaderValue());
  auto client = TestLoadWithHeaders(CreateFileSystemURL("bigfile"), &headers);
  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  // Don't use EXPECT_EQ, it will print out a lot of garbage if check failed.
  EXPECT_TRUE(partial_buffer == base::as_byte_span(response_text));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest,
                       FileTestMultipleRangesNotSupported) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile(
      "file1.dat",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    "bytes=0-5,10-200,200-300");
  auto client = TestLoadWithHeaders(CreateFileSystemURL("file1.dat"), &headers);
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileRangeOutOfBounds) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile(
      "file1.dat",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));
  net::HttpRequestHeaders headers;
  headers.SetHeader(net::HttpRequestHeaders::kRange,
                    net::HttpByteRange::Bounded(500, 1000).GetHeaderValue());
  auto client = TestLoadWithHeaders(CreateFileSystemURL("file1.dat"), &headers);

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_REQUEST_RANGE_NOT_SATISFIABLE,
            client->completion_status().error_code);
}

// This test times out: http://crbug.com/944647.
IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest,
                       DISABLED_FileDirRedirect) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  CreateDirectory("dir");
  auto client = TestLoad(CreateFileSystemURL("dir"));

  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_TRUE(client->has_received_redirect());
  EXPECT_EQ(301, client->redirect_info().status_code);
  EXPECT_EQ(CreateFileSystemURL("dir/"), client->redirect_info().new_url);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileNoSuchRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(GURL("filesystem:http://remote/persistent/somefile"));
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, NoSuchFile) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  auto client = TestLoad(CreateFileSystemURL("somefile"));
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileCancel) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile(
      "file1.dat",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));
  auto client = TestLoadNoRun(CreateFileSystemURL("file1.dat"));

  // client.reset();
  base::RunLoop().RunUntilIdle();
  // If we get here, success! we didn't crash!
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileGetMimeType) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  std::string file_data =
      "<!DOCTYPE HTML><html><head>test</head>"
      "<body>foo</body></html>";
  const char kFilename[] = "hoge.html";
  WriteFile(kFilename, base::as_byte_span(file_data));

  std::string mime_type_direct;
  base::FilePath::StringType extension =
      base::FilePath().AppendASCII(kFilename).Extension();
  if (!extension.empty())
    extension = extension.substr(1);
  EXPECT_TRUE(
      net::GetWellKnownMimeTypeFromExtension(extension, &mime_type_direct));

  auto client = TestLoad(CreateFileSystemURL(kFilename));
  EXPECT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());

  EXPECT_EQ(mime_type_direct, client->response_head()->mime_type);
  EXPECT_TRUE(client->response_head()->did_mime_sniff);
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileIncognito) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  WriteFile(
      "file",
      base::as_byte_span(kTestFileData).first(std::size(kTestFileData) - 1));

  // Creates a new filesystem context for incognito mode.
  scoped_refptr<FileSystemContext> file_system_context =
      storage::CreateIncognitoFileSystemContextForTesting(
          io_task_runner(), blocking_task_runner(), nullptr,
          temp_dir_.GetPath());

  // The request should return NOT_FOUND error if it's in incognito mode.
  auto client = TestLoadWithContext(CreateFileSystemURL("file"),
                                    file_system_context.get());
  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  // Make sure it returns success with regular (non-incognito) context.
  client = TestLoad(CreateFileSystemURL("file"));
  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  EXPECT_EQ(200, client->response_head()->headers->response_code());
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileAutoMountFileTest) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetUpFileAutoMountContext();
  auto client =
      TestLoad(GURL("filesystem:http://automount/external/mnt_name/foo"));

  ASSERT_TRUE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  std::string response_text = ReadDataPipe(client->response_body_release());
  EXPECT_EQ(kTestFileData, response_text);
  EXPECT_EQ(200, client->response_head()->headers->response_code());

  std::string cache_control;
  EXPECT_TRUE(client->response_head()->headers->GetNormalizedHeader(
      "cache-control", &cache_control));
  EXPECT_EQ("no-cache", cache_control);

  ASSERT_TRUE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest,
                       FileAutoMountInvalidRoot) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetUpFileAutoMountContext();
  auto client =
      TestLoad(GURL("filesystem:http://automount/external/invalid/foo"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  ASSERT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          "invalid"));
}

IN_PROC_BROWSER_TEST_P(FileSystemURLLoaderFactoryTest, FileAutoMountNoHandler) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SetUpFileAutoMountContext();
  auto client =
      TestLoad(GURL("filesystem:http://noauto/external/mnt_name/foo"));

  EXPECT_FALSE(client->has_received_response());
  EXPECT_TRUE(client->has_received_completion());
  EXPECT_EQ(net::ERR_FILE_NOT_FOUND, client->completion_status().error_code);

  ASSERT_FALSE(
      storage::ExternalMountPoints::GetSystemInstance()->RevokeFileSystem(
          kValidExternalMountPoint));
}

}  // namespace content
