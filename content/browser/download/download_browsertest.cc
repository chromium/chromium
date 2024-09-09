// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains download browser tests that are known to be runnable
// in a pure content context.  Over time tests should be migrated here.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_item.h"
#include "components/download/public/common/download_item_impl.h"
#include "components/download/public/common/download_stats.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/parallel_download_configs.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/test_download_http_response.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/url_loader_monitor.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "net/base/features.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_connection_info.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/switches.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "base/android/build_info.h"
#endif

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::Values;

namespace net {
class NetLogWithSource;
}

namespace content {

namespace {

// Default request count for parallel download tests.
constexpr int kTestRequestCount = 3;

// Offset for download to pause.
const int kPauseOffset = 100 * 1024;

const char kOriginOne[] = "one.example";
const char kOriginTwo[] = "two.example";
const char kOrigin[] = "example.com";
const char kOriginSubdomain[] = "subdomain.example.com";
const char kOtherOrigin[] = "example.site";
const char kBlogspotSite1[] = "a.blogspot.com";
const char kBlogspotSite2[] = "b.blogspot.com";

const char k404Response[] = "HTTP/1.1 404 Not found\r\n\r\n";

void ExpectRequestIsolationInfo(
    const GURL& request_url,
    const net::IsolationInfo& expected_isolation_info,
    base::OnceCallback<void()> function) {
  URLLoaderMonitor monitor({request_url});

  std::move(function).Run();
  monitor.WaitForUrls();

  std::optional<network::ResourceRequest> request =
      monitor.GetRequestInfo(request_url);
  ASSERT_TRUE(request->trusted_params.has_value());
  EXPECT_TRUE(expected_isolation_info.IsEqualForTesting(
      request->trusted_params->isolation_info));
  // SiteForCookies should be consistent with the NIK.
  EXPECT_TRUE(expected_isolation_info.site_for_cookies().IsEquivalent(
      request->site_for_cookies));
}

// Implementation of TestContentBrowserClient that overrides
// AllowRenderingMhtmlOverHttp() and allows consumers to set a value.
class DownloadTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  DownloadTestContentBrowserClient() {
#if BUILDFLAG(IS_ANDROID)
    content_url_loader_factory_ = std::make_unique<FakeNetworkURLLoaderFactory>(
        "HTTP/1.1 200 OK\nContent-Type: multipart/related\n\n",
        "This is a test for download mhtml through non http/https urls",
        /* network_accessed */ true, net::OK);
#endif  // BUILDFLAG(IS_ANDROID)

    file_url_loader_factory_ = std::make_unique<FakeNetworkURLLoaderFactory>(
        "HTTP/1.1 200 OK\nContent-Type: multipart/related\n\n",
        "This is a test for download mhtml through non http/https urls",
        /* network_accessed */ true, net::OK);
  }

  DownloadTestContentBrowserClient(const DownloadTestContentBrowserClient&) =
      delete;
  DownloadTestContentBrowserClient& operator=(
      const DownloadTestContentBrowserClient&) = delete;

  bool AllowRenderingMhtmlOverHttp(NavigationUIData* navigation_data) override {
    return allowed_rendering_mhtml_over_http_;
  }

  void set_allowed_rendering_mhtml_over_http(bool allowed) {
    allowed_rendering_mhtml_over_http_ = allowed;
  }

  void enable_register_non_network_url_loader(bool enabled) {
    enable_register_non_network_url_loader_ = enabled;
  }

  base::FilePath GetDefaultDownloadDirectory() override {
    return base::FilePath();
  }

  mojo::PendingRemote<network::mojom::URLLoaderFactory>
  CreateNonNetworkNavigationURLLoaderFactory(
      const std::string& scheme,
      FrameTreeNodeId frame_tree_node_id) override {
    if (!enable_register_non_network_url_loader_) {
      return {};
    }

#if BUILDFLAG(IS_ANDROID)
    if (scheme == url::kContentScheme) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory>
          content_factory_remote;
      content_url_loader_factory_->Clone(
          content_factory_remote.InitWithNewPipeAndPassReceiver());
      return content_factory_remote;
    }
#endif  // BUILDFLAG(IS_ANDROID)

    if (scheme == url::kFileScheme) {
      mojo::PendingRemote<network::mojom::URLLoaderFactory> file_factory_remote;
      file_url_loader_factory_->Clone(
          file_factory_remote.InitWithNewPipeAndPassReceiver());
      return file_factory_remote;
    }

    return {};
  }

 private:
  bool allowed_rendering_mhtml_over_http_ = false;
  bool enable_register_non_network_url_loader_ = false;

  std::unique_ptr<FakeNetworkURLLoaderFactory> content_url_loader_factory_;
  std::unique_ptr<FakeNetworkURLLoaderFactory> file_url_loader_factory_;
};

class MockDownloadItemObserver : public download::DownloadItem::Observer {
 public:
  MockDownloadItemObserver() {}
  ~MockDownloadItemObserver() override {}

  MOCK_METHOD1(OnDownloadUpdated, void(download::DownloadItem*));
  MOCK_METHOD1(OnDownloadOpened, void(download::DownloadItem*));
  MOCK_METHOD1(OnDownloadRemoved, void(download::DownloadItem*));
  MOCK_METHOD1(OnDownloadDestroyed, void(download::DownloadItem*));
};

class MockDownloadManagerObserver : public DownloadManager::Observer {
 public:
  explicit MockDownloadManagerObserver(DownloadManager* manager) {
    manager_ = manager;
    manager->AddObserver(this);
  }
  ~MockDownloadManagerObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  MOCK_METHOD2(OnDownloadCreated,
               void(DownloadManager*, download::DownloadItem*));
  MOCK_METHOD1(OnDownloadDropped, void(DownloadManager*));
  MOCK_METHOD1(ModelChanged, void(DownloadManager*));
  void ManagerGoingDown(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    MockManagerGoingDown(manager);

    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  MOCK_METHOD1(MockManagerGoingDown, void(DownloadManager*));

 private:
  raw_ptr<DownloadManager> manager_;
};

class DownloadFileWithDelayFactory;

static DownloadManagerImpl* DownloadManagerForShell(Shell* shell) {
  // We're in a content_browsertest; we know that the DownloadManager
  // is a DownloadManagerImpl.
  return static_cast<DownloadManagerImpl*>(
      shell->web_contents()->GetBrowserContext()->GetDownloadManager());
}

class DownloadFileWithDelay : public download::DownloadFileImpl {
 public:
  DownloadFileWithDelay(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer,
      base::WeakPtr<DownloadFileWithDelayFactory> owner);

  DownloadFileWithDelay(const DownloadFileWithDelay&) = delete;
  DownloadFileWithDelay& operator=(const DownloadFileWithDelay&) = delete;

  ~DownloadFileWithDelay() override;

  // Wraps DownloadFileImpl::Rename* and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  void RenameAndUniquify(const base::FilePath& full_path,
                         RenameCompletionCallback callback) override;
  void RenameAndAnnotate(
      const base::FilePath& full_path,
      const std::string& client_guid,
      const GURL& source_url,
      const GURL& referrer_url,
      const std::optional<url::Origin>& request_initiator,
      mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
      RenameCompletionCallback callback) override;

 private:
  static void RenameCallbackWrapper(
      const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
      RenameCompletionCallback original_callback,
      download::DownloadInterruptReason reason,
      const base::FilePath& path);

  // This variable may only be read on the download sequence, and may only be
  // indirected through (e.g. methods on DownloadFileWithDelayFactory called)
  // on the UI thread.  This is because after construction,
  // DownloadFileWithDelay lives on the file thread, but
  // DownloadFileWithDelayFactory is purely a UI thread object.
  base::WeakPtr<DownloadFileWithDelayFactory> owner_;
};

// All routines on this class must be called on the UI thread.
class DownloadFileWithDelayFactory : public download::DownloadFileFactory {
 public:
  DownloadFileWithDelayFactory();

  DownloadFileWithDelayFactory(const DownloadFileWithDelayFactory&) = delete;
  DownloadFileWithDelayFactory& operator=(const DownloadFileWithDelayFactory&) =
      delete;

  ~DownloadFileWithDelayFactory() override;

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      const base::FilePath& duplicate_download_file_path,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override;

  void AddRenameCallback(base::OnceClosure callback);
  void GetAllRenameCallbacks(std::vector<base::OnceClosure>* results);

  // Do not return until GetAllRenameCallbacks() will return a non-empty list.
  void WaitForSomeCallback();

 private:
  std::vector<base::OnceClosure> rename_callbacks_;
  base::OnceClosure stop_waiting_;
  base::WeakPtrFactory<DownloadFileWithDelayFactory> weak_ptr_factory_{this};
};

DownloadFileWithDelay::DownloadFileWithDelay(
    std::unique_ptr<download::DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<download::InputStream> stream,
    uint32_t download_id,
    base::WeakPtr<download::DownloadDestinationObserver> observer,
    base::WeakPtr<DownloadFileWithDelayFactory> owner)
    : download::DownloadFileImpl(std::move(save_info),
                                 default_download_directory,
                                 std::move(stream),
                                 download_id,
                                 observer),
      owner_(owner) {}

DownloadFileWithDelay::~DownloadFileWithDelay() {}

void DownloadFileWithDelay::RenameAndUniquify(
    const base::FilePath& full_path,
    RenameCompletionCallback callback) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  download::DownloadFileImpl::RenameAndUniquify(
      full_path, base::BindOnce(DownloadFileWithDelay::RenameCallbackWrapper,
                                owner_, std::move(callback)));
}

void DownloadFileWithDelay::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const std::optional<url::Origin>& request_initiator,
    mojo::PendingRemote<quarantine::mojom::Quarantine> remote_quarantine,
    RenameCompletionCallback callback) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  download::DownloadFileImpl::RenameAndAnnotate(
      full_path, client_guid, source_url, referrer_url, request_initiator,
      mojo::NullRemote(),
      base::BindOnce(DownloadFileWithDelay::RenameCallbackWrapper, owner_,
                     std::move(callback)));
}

// static
void DownloadFileWithDelay::RenameCallbackWrapper(
    const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
    RenameCompletionCallback original_callback,
    download::DownloadInterruptReason reason,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!factory)
    return;
  factory->AddRenameCallback(
      base::BindOnce(std::move(original_callback), reason, path));
}

DownloadFileWithDelayFactory::DownloadFileWithDelayFactory() {}

DownloadFileWithDelayFactory::~DownloadFileWithDelayFactory() {}

download::DownloadFile* DownloadFileWithDelayFactory::CreateFile(
    std::unique_ptr<download::DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<download::InputStream> stream,
    uint32_t download_id,
    const base::FilePath& duplicate_download_file_path,
    base::WeakPtr<download::DownloadDestinationObserver> observer) {
  return new DownloadFileWithDelay(
      std::move(save_info), default_download_directory, std::move(stream),
      download_id, observer, weak_ptr_factory_.GetWeakPtr());
}

void DownloadFileWithDelayFactory::AddRenameCallback(
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  rename_callbacks_.push_back(std::move(callback));
  if (stop_waiting_)
    std::move(stop_waiting_).Run();
}

void DownloadFileWithDelayFactory::GetAllRenameCallbacks(
    std::vector<base::OnceClosure>* results) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  results->swap(rename_callbacks_);
}

void DownloadFileWithDelayFactory::WaitForSomeCallback() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (rename_callbacks_.empty()) {
    base::RunLoop run_loop;
    stop_waiting_ = run_loop.QuitClosure();
    run_loop.Run();
  }
}

class CountingDownloadFile : public download::DownloadFileImpl {
 public:
  CountingDownloadFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer)
      : download::DownloadFileImpl(std::move(save_info),
                                   default_downloads_directory,
                                   std::move(stream),
                                   download_id,
                                   observer) {}

  ~CountingDownloadFile() override {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    active_files_--;
  }

  void Initialize(
      InitializeCallback callback,
      CancelRequestCallback cancel_request_callback,
      const download::DownloadItem::ReceivedSlices& received_slices) override {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    active_files_++;
    download::DownloadFileImpl::Initialize(std::move(callback),
                                           std::move(cancel_request_callback),
                                           received_slices);
  }

  static void GetNumberActiveFiles(int* result) {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    *result = active_files_;
  }

  // Can be called on any thread, and will block (running message loop)
  // until data is returned.
  static int GetNumberActiveFilesFromFileThread() {
    int result = -1;
    base::RunLoop run_loop;
    download::GetDownloadTaskRunner()->PostTaskAndReply(
        FROM_HERE,
        base::BindOnce(&CountingDownloadFile::GetNumberActiveFiles, &result),
        run_loop.QuitClosure());
    run_loop.Run();
    DCHECK_NE(-1, result);
    return result;
  }

 private:
  static int active_files_;
};

int CountingDownloadFile::active_files_ = 0;

class CountingDownloadFileFactory : public download::DownloadFileFactory {
 public:
  CountingDownloadFileFactory() {}
  ~CountingDownloadFileFactory() override {}

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      const base::FilePath& duplicate_download_file_path,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override {
    return new CountingDownloadFile(std::move(save_info),
                                    default_downloads_directory,
                                    std::move(stream), download_id, observer);
  }
};

class ErrorInjectionDownloadFile : public download::DownloadFileImpl {
 public:
  ErrorInjectionDownloadFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_downloads_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer,
      int64_t error_stream_offset,
      int64_t error_stream_length)
      : download::DownloadFileImpl(std::move(save_info),
                                   default_downloads_directory,
                                   std::move(stream),
                                   download_id,
                                   observer),
        error_stream_offset_(error_stream_offset),
        error_stream_length_(error_stream_length) {}

  ~ErrorInjectionDownloadFile() override = default;

  void InjectStreamError(int64_t error_stream_offset,
                         int64_t error_stream_length) {
    error_stream_offset_ = error_stream_offset;
    error_stream_length_ = error_stream_length;
  }

  download::DownloadInterruptReason HandleStreamCompletionStatus(
      SourceStream* source_stream) override {
    if (source_stream->offset() == error_stream_offset_ &&
        source_stream->bytes_written() >= error_stream_length_) {
      return download::DOWNLOAD_INTERRUPT_REASON_SERVER_FAILED;
    }
    return download::DownloadFileImpl::HandleStreamCompletionStatus(
        source_stream);
  }

 private:
  int64_t error_stream_offset_;
  int64_t error_stream_length_;
};

// Factory for creating download files that allow error injection. All routines
// on this class must be called on the UI thread.
class ErrorInjectionDownloadFileFactory : public download::DownloadFileFactory {
 public:
  ErrorInjectionDownloadFileFactory() : download_file_(nullptr) {}

  ErrorInjectionDownloadFileFactory(const ErrorInjectionDownloadFileFactory&) =
      delete;
  ErrorInjectionDownloadFileFactory& operator=(
      const ErrorInjectionDownloadFileFactory&) = delete;

  ~ErrorInjectionDownloadFileFactory() override = default;

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      const base::FilePath& duplicate_download_file_path,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override {
    ErrorInjectionDownloadFile* download_file = new ErrorInjectionDownloadFile(
        std::move(save_info), default_download_directory, std::move(stream),
        download_id, observer, injected_error_offset_, injected_error_length_);
    // If the InjectError() is not called yet, memorize |download_file| and wait
    // for error to be injected.
    if (injected_error_offset_ < 0)
      download_file_ = download_file;
    injected_error_offset_ = -1;
    injected_error_length_ = 0;
    return download_file;
  }

  void InjectError(int64_t offset, int64_t length) {
    injected_error_offset_ = offset;
    injected_error_length_ = length;
    if (!download_file_)
      return;
    InjectErrorIntoDownloadFile();
  }

  base::WeakPtr<ErrorInjectionDownloadFileFactory> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  void InjectErrorIntoDownloadFile() {
    download::GetDownloadTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(&ErrorInjectionDownloadFile::InjectStreamError,
                       base::Unretained(download_file_), injected_error_offset_,
                       injected_error_length_));
    injected_error_offset_ = -1;
    injected_error_length_ = 0;
    download_file_ = nullptr;
  }

  raw_ptr<ErrorInjectionDownloadFile, AcrossTasksDanglingUntriaged>
      download_file_;
  int64_t injected_error_offset_ = -1;
  int64_t injected_error_length_ = 0;
  base::WeakPtrFactory<ErrorInjectionDownloadFileFactory> weak_ptr_factory_{
      this};
};

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  TestShellDownloadManagerDelegate()
      : delay_download_open_(false) {}
  ~TestShellDownloadManagerDelegate() override {}

  bool ShouldOpenDownload(download::DownloadItem* item,
                          DownloadOpenDelayedCallback callback) override {
    if (delay_download_open_) {
      delayed_callbacks_.push_back(std::move(callback));
      return false;
    }
    return true;
  }

  void SetDelayedOpen(bool delay) {
    delay_download_open_ = delay;
  }

  void GetDelayedCallbacks(
      std::vector<DownloadOpenDelayedCallback>* callbacks) {
    callbacks->swap(delayed_callbacks_);
  }

 private:
  bool delay_download_open_;
  std::vector<DownloadOpenDelayedCallback> delayed_callbacks_;
};

// Get the next created download.
class DownloadCreateObserver : DownloadManager::Observer {
 public:
  explicit DownloadCreateObserver(DownloadManager* manager)
      : manager_(manager), item_(nullptr) {
    manager_->AddObserver(this);
  }

  ~DownloadCreateObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void ManagerGoingDown(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* download) override {
    if (!item_)
      item_ = download;

    if (completion_closure_)
      std::move(completion_closure_).Run();
  }

  download::DownloadItem* WaitForFinished() {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    if (!item_) {
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    return item_;
  }

 private:
  raw_ptr<DownloadManager> manager_;
  raw_ptr<download::DownloadItem> item_;
  base::OnceClosure completion_closure_;
};

class DownloadInProgressObserver : public DownloadTestObserverInProgress {
 public:
  explicit DownloadInProgressObserver(DownloadManager* manager)
      : DownloadTestObserverInProgress(manager, 1 /* wait_count */),
        manager_(manager) {}

  download::DownloadItem* WaitAndGetInProgressDownload() {
    DownloadTestObserverInProgress::WaitForFinished();

    DownloadManager::DownloadVector items;
    manager_->GetAllDownloads(&items);

    download::DownloadItem* download_item = nullptr;
    for (auto iter = items.begin(); iter != items.end(); ++iter) {
      if ((*iter)->GetState() == download::DownloadItem::IN_PROGRESS) {
        // There should be only one IN_PROGRESS item.
        EXPECT_FALSE(download_item);
        download_item = *iter;
      }
    }
    EXPECT_TRUE(download_item);
    EXPECT_EQ(download::DownloadItem::IN_PROGRESS, download_item->GetState());
    return download_item;
  }

 private:
  raw_ptr<DownloadManager> manager_;
};

class DownloadCountingObserver : public download::DownloadItem::Observer {
 public:
  DownloadCountingObserver() : item_(nullptr), count_(0) {}

  ~DownloadCountingObserver() override {
    if (item_)
      item_->RemoveObserver(this);
  }

  void OnDownloadUpdated(download::DownloadItem* download) override {
    if (IsCountReached(download, count_) && completion_closure_)
      std::move(completion_closure_).Run();
  }

  void OnDownloadDestroyed(download::DownloadItem* download) override {
    item_ = nullptr;
  }

  void WaitForFinished(download::DownloadItem* item, int count) {
    if (IsCountReached(item, count))
      return;
    item_ = item;
    count_ = count;
    if (item_) {
      item_->AddObserver(this);
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 protected:
  virtual bool IsCountReached(download::DownloadItem* download, int count) = 0;

 private:
  raw_ptr<download::DownloadItem> item_;
  int count_;
  base::OnceClosure completion_closure_;
};

class ReceivedSlicesCountingObserver : public DownloadCountingObserver {
 private:
  bool IsCountReached(download::DownloadItem* download, int count) override {
    return download->GetReceivedSlices().size() >= static_cast<size_t>(count);
  }
};

class ErrorStreamCountingObserver : public DownloadCountingObserver {
 private:
  bool IsCountReached(download::DownloadItem* download, int count) override {
    return download::GetParallelRequestCreationFailureCountForTesting() ==
           count;
  }

 private:
  base::HistogramTester histogram_tester_;
};

class ReceivedBytesCountingObserver : public DownloadCountingObserver {
 private:
  bool IsCountReached(download::DownloadItem* download, int count) override {
    return download->GetReceivedBytes() == count;
  }
};

// Class to wait for a WebContents to kick off a specified number of
// navigations.
class NavigationStartObserver : public WebContentsObserver {
 public:
  explicit NavigationStartObserver(WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  NavigationStartObserver(const NavigationStartObserver&) = delete;
  NavigationStartObserver& operator=(const NavigationStartObserver&) = delete;

  ~NavigationStartObserver() override {}

  void WaitForFinished(int navigation_count) {
    if (start_count_ >= navigation_count)
      return;
    navigation_count_ = navigation_count;
    base::RunLoop run_loop;
    completion_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // WebContentsObserver implementations.
  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    start_count_++;
    if (start_count_ >= navigation_count_ && completion_closure_) {
      std::move(completion_closure_).Run();
    }
  }

  int navigation_count_ = 0;
  int start_count_ = 0;
  base::OnceClosure completion_closure_;
};

bool IsDownloadInState(download::DownloadItem::DownloadState state,
                       download::DownloadItem* item) {
  return item->GetState() == state;
}

// Request handler to be used with CreateRedirectHandler().
std::unique_ptr<net::test_server::HttpResponse>
HandleRequestAndSendRedirectResponse(
    const std::string& relative_url,
    const GURL& target_url,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_FOUND);
    response->AddCustomHeader("Location", target_url.spec());
  }
  return std::move(response);
}

// Creates a request handler for EmbeddedTestServer that responds with a HTTP
// 302 redirect if the request URL matches |relative_url|.
net::EmbeddedTestServer::HandleRequestCallback CreateRedirectHandler(
    const std::string& relative_url,
    const GURL& target_url) {
  return base::BindRepeating(&HandleRequestAndSendRedirectResponse,
                             relative_url, target_url);
}

// Request handler to be used with CreateBasicResponseHandler().
std::unique_ptr<net::test_server::HttpResponse>
HandleRequestAndSendBasicResponse(
    const std::string& relative_url,
    net::HttpStatusCode code,
    const base::StringPairs& headers,
    const std::string& content_type,
    const std::string& body,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response = std::make_unique<net::test_server::BasicHttpResponse>();
    for (const auto& pair : headers)
      response->AddCustomHeader(pair.first, pair.second);
    response->set_content_type(content_type);
    response->set_content(body);
    response->set_code(code);
  }
  return std::move(response);
}

// Creates a request handler for an EmbeddedTestServer that response with an
// HTTP 200 status code, a Content-Type header and a body.
net::EmbeddedTestServer::HandleRequestCallback CreateBasicResponseHandler(
    const std::string& relative_url,
    net::HttpStatusCode code,
    const base::StringPairs& headers,
    const std::string& content_type,
    const std::string& body) {
  return base::BindRepeating(&HandleRequestAndSendBasicResponse, relative_url,
                             code, headers, content_type, body);
}

std::unique_ptr<net::test_server::HttpResponse> HandleRequestAndEchoCookies(
    const std::string& relative_url,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->AddCustomHeader("Content-Disposition", "attachment");
    response->AddCustomHeader("Vary", "");
    response->AddCustomHeader("Cache-Control", "no-cache");
    response->set_content_type("text/plain");
    response->set_content(request.headers.at("cookie"));
  }
  return std::move(response);
}

// Creates a request handler for an EmbeddedTestServer that echos the value
// of the cookie header back as a body, and sends a Content-Disposition header.
net::EmbeddedTestServer::HandleRequestCallback CreateEchoCookieHandler(
    const std::string& relative_url) {
  return base::BindRepeating(&HandleRequestAndEchoCookies, relative_url);
}

// A request handler that takes the content of the request and sends it back on
// the response.
std::unique_ptr<net::test_server::HttpResponse> HandleUploadRequest(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response(
      (new net::test_server::BasicHttpResponse()));
  response->set_content(request.content);
  return std::move(response);
}

// Helper class to "flatten" handling of
// TestDownloadHttpResponse::OnPauseHandler.
class TestRequestPauseHandler {
 public:
  // Construct an OnPauseHandler that can be set as the on_pause_handler for
  // TestDownloadHttpResponse::Parameters.
  TestDownloadHttpResponse::OnPauseHandler GetOnPauseHandler() {
    EXPECT_FALSE(used_) << "GetOnPauseHandler() should only be called once for "
                           "an instance of TestRequestPauseHandler.";
    used_ = true;
    return base::BindRepeating(&TestRequestPauseHandler::OnPauseHandler,
                               base::Unretained(this));
  }

  // Wait until the OnPauseHandler returned in a prior call to
  // GetOnPauseHandler() is invoked.
  void WaitForCallback() {
    if (resume_callback_.is_null())
      run_loop_.Run();
  }

  // Resume the server response.
  void Resume() {
    ASSERT_FALSE(resume_callback_.is_null());
    std::move(resume_callback_).Run();
  }

 private:
  void OnPauseHandler(base::OnceClosure resume_callback) {
    resume_callback_ = std::move(resume_callback);
    if (run_loop_.running())
      run_loop_.Quit();
  }

  bool used_ = false;
  base::RunLoop run_loop_;
  base::OnceClosure resume_callback_;
};

class DownloadContentTest : public ContentBrowserTest {
 public:
  DownloadContentTest() {
    feature_list_.InitWithFeatures(
        {},
        {
            download::features::kAllowDownloadResumptionWithoutStrongValidators,
            // Link Preview hides alt+click. Disables it not to do so.
            blink::features::kLinkPreview,
        });
  }

 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    test_delegate_ = std::make_unique<TestShellDownloadManagerDelegate>();
    test_delegate_->SetDownloadBehaviorForTesting(
        downloads_directory_.GetPath());
    DownloadManager* manager = DownloadManagerForShell(shell());
    manager->GetDelegate()->Shutdown();
    manager->SetDelegate(test_delegate_.get());
    test_delegate_->SetDownloadManager(manager);

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SlowDownloadHttpResponse::HandleSlowDownloadRequest));
    test_response_handler_.RegisterToTestServer(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    const std::string real_host =
        embedded_test_server()->host_port_pair().host();
    host_resolver()->AddRule(kOriginOne, real_host);
    host_resolver()->AddRule(kOriginTwo, real_host);
    host_resolver()->AddRule(kOrigin, real_host);
    host_resolver()->AddRule(kOriginSubdomain, real_host);
    host_resolver()->AddRule(kOtherOrigin, real_host);
    host_resolver()->AddRule(kBlogspotSite1, real_host);
    host_resolver()->AddRule(kBlogspotSite2, real_host);
    host_resolver()->AddRule(SlowDownloadHttpResponse::kSlowResponseHostName,
                             real_host);
    host_resolver()->AddRule(TestDownloadHttpResponse::kTestDownloadHostName,
                             real_host);
    host_resolver()->AddRule("a.test", "127.0.0.1");
    host_resolver()->AddRule("b.test", "127.0.0.1");
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
    // Some tests are flaky due to slower loading interacting with deferred
    // commits so allow early input.
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  TestShellDownloadManagerDelegate* GetDownloadManagerDelegate() {
    return test_delegate_.get();
  }

  const base::FilePath& GetDownloadDirectory() const {
    return downloads_directory_.GetPath();
  }

  // Create a DownloadTestObserverTerminal that will wait for the
  // specified number of downloads to finish.
  DownloadTestObserver* CreateWaiter(
      Shell* shell, int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverTerminal(download_manager, num_downloads,
        DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  }

  // Create a DownloadTestObserverInProgress that will wait for the
  // specified number of downloads to start.
  DownloadTestObserver* CreateInProgressWaiter(Shell* shell,
                                               int num_downloads) {
    DownloadManager* download_manager = DownloadManagerForShell(shell);
    return new DownloadTestObserverInProgress(download_manager, num_downloads);
  }

  void WaitForInterrupt(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::BindRepeating(&IsDownloadInState,
                                      download::DownloadItem::INTERRUPTED))
        .WaitForEvent();
  }

  void WaitForInProgress(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::BindRepeating(&IsDownloadInState,
                                      download::DownloadItem::IN_PROGRESS))
        .WaitForEvent();
  }

  void WaitForCompletion(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::BindRepeating(&IsDownloadInState,
                                      download::DownloadItem::COMPLETE))
        .WaitForEvent();
  }

  void WaitForCancel(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download, base::BindRepeating(&IsDownloadInState,
                                      download::DownloadItem::CANCELLED))
        .WaitForEvent();
  }

  // Note: Cannot be used with other alternative DownloadFileFactorys
  void SetupEnsureNoPendingDownloads() {
    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        std::unique_ptr<download::DownloadFileFactory>(
            new CountingDownloadFileFactory()));
  }

  bool EnsureNoPendingDownloads() {
    return CountingDownloadFile::GetNumberActiveFilesFromFileThread() == 0;
  }

  void SetupErrorInjectionDownloads() {
    auto factory = std::make_unique<ErrorInjectionDownloadFileFactory>();
    inject_error_callback_ = base::BindRepeating(
        &ErrorInjectionDownloadFileFactory::InjectError, factory->GetWeakPtr());

    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        std::move(factory));
  }

  // Navigate to a URL and wait for a download, expecting that the URL will not
  // result in a new committed navigation. This is typically the case for most
  // downloads.
  void NavigateToURLAndWaitForDownload(
      Shell* shell,
      const GURL& url,
      download::DownloadItem::DownloadState expected_terminal_state) {
    std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    EXPECT_TRUE(NavigateToURLAndExpectNoCommit(shell, url));
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(expected_terminal_state));
  }

  // Navigate to a URL, expecting it to commit and donn't canceled by download.
  // This is useful when the URL actually commits and donn't start any download.
  void NavigateToCommittedURLAndExpectNoDownload(Shell* shell,
                                                 const GURL& url) {
    EXPECT_TRUE(NavigateToURL(shell, url));
  }

  // Navigate to a URL, expecting it to commit, and wait for a download. This
  // is useful when the URL actually commits and then starts a download via
  // script.
  void NavigateToCommittedURLAndWaitForDownload(
      Shell* shell,
      const GURL& url,
      download::DownloadItem::DownloadState expected_terminal_state) {
    std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    EXPECT_TRUE(NavigateToURL(shell, url));
    observer->WaitForFinished();
    EXPECT_EQ(1u, observer->NumDownloadsSeenInState(expected_terminal_state));
  }

  // Checks that |path| is has |file_size| bytes, and matches the |value|
  // string.
  bool VerifyFile(const base::FilePath& path,
                  const std::string& value,
                  const int64_t file_size) {
    std::string file_contents;

    {
      base::ScopedAllowBlockingForTesting allow_blocking;
      bool read = base::ReadFileToString(path, &file_contents);
      EXPECT_TRUE(read) << "Failed reading file: " << path.value() << std::endl;
      if (!read)
        return false;  // Couldn't read the file.
    }

    // Note: we don't handle really large files (more than size_t can hold)
    // so we will fail in that case.
    size_t expected_size = static_cast<size_t>(file_size);

    // Check the size.
    EXPECT_EQ(expected_size, file_contents.size());
    if (expected_size != file_contents.size())
      return false;

    // Check the contents.
    EXPECT_EQ(value, file_contents);
    if (memcmp(file_contents.c_str(), value.c_str(), expected_size) != 0)
      return false;

    return true;
  }

  // Start a download and return the item.
  download::DownloadItem* StartDownloadAndReturnItem(Shell* shell, GURL url) {
    std::unique_ptr<DownloadCreateObserver> observer(
        new DownloadCreateObserver(DownloadManagerForShell(shell)));
    shell->LoadURL(url);
    return observer->WaitForFinished();
  }

  TestDownloadResponseHandler* test_response_handler() {
    return &test_response_handler_;
  }

  static bool PathExists(const base::FilePath& path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    return base::PathExists(path);
  }

  static void ReadAndVerifyFileContents(int seed,
                                        int64_t expected_size,
                                        const base::FilePath& path) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::File file(path, base::File::FLAG_OPEN | base::File::FLAG_READ);
    ASSERT_TRUE(file.IsValid());
    int64_t file_length = file.GetLength();
    ASSERT_EQ(expected_size, file_length);

    const int64_t kBufferSize = 64 * 1024;
    std::string pattern;
    std::vector<char> data;
    pattern.resize(kBufferSize);
    data.resize(kBufferSize);
    for (int64_t offset = 0; offset < file_length;) {
      int bytes_read =
          UNSAFE_TODO(file.Read(offset, &data.front(), kBufferSize));
      ASSERT_LT(0, bytes_read);
      ASSERT_GE(kBufferSize, bytes_read);

      pattern =
          TestDownloadHttpResponse::GetPatternBytes(seed, offset, bytes_read);
      ASSERT_EQ(0, memcmp(pattern.data(), &data.front(), bytes_read))
          << "Comparing block at offset " << offset << " and length "
          << bytes_read;
      offset += bytes_read;
    }
  }

  TestDownloadHttpResponse::InjectErrorCallback inject_error_callback() {
    return inject_error_callback_;
  }

  void RegisterServiceWorker(Shell* shell, const std::string& worker_url) {
    EXPECT_TRUE(NavigateToURL(shell, embedded_test_server()->GetURL(
                                         "/register_service_worker.html")));
    EXPECT_EQ("DONE", EvalJs(shell, "register('" + worker_url + "')"));
  }

  void ClearAutoResumptionCount(download::DownloadItem* download) {
    static_cast<download::DownloadItemImpl*>(download)
        ->SetAutoResumeCountForTesting(0);
  }

 private:
  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
  std::unique_ptr<TestShellDownloadManagerDelegate> test_delegate_;
  TestDownloadResponseHandler test_response_handler_;
  TestDownloadHttpResponse::InjectErrorCallback inject_error_callback_;
  base::test::ScopedFeatureList feature_list_;
};

constexpr int kValidationLength = 1024;

class DownloadContentTestWithoutStrongValidators : public DownloadContentTest {
 public:
  DownloadContentTestWithoutStrongValidators() {
    std::map<std::string, std::string> params = {
        {download::kDownloadContentValidationLengthFinchKey,
         base::NumberToString(kValidationLength)}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        download::features::kAllowDownloadResumptionWithoutStrongValidators,
        params);
  }

  // Starts a download without strong validators, interrupts it, and resumes it.
  // If |fail_content_validation| is true, download content will change during
  // resumption.
  void InterruptAndResumeDownloadWithoutStrongValidators(
      bool fail_content_validation) {
    SetupErrorInjectionDownloads();
    GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
    GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
    TestDownloadHttpResponse::Parameters parameters =
        TestDownloadHttpResponse::Parameters::WithSingleInterruption(
            inject_error_callback());
    parameters.etag.clear();
    parameters.last_modified.clear();
    TestDownloadHttpResponse::StartServing(parameters, server_url);

    int64_t interruption_offset = parameters.injected_errors.front();
    download::DownloadItem* download =
        StartDownloadAndReturnItem(shell(), server_url);
    WaitForInterrupt(download);

    ASSERT_EQ(interruption_offset, download->GetReceivedBytes());
    ASSERT_EQ(parameters.size, download->GetTotalBytes());

    parameters.ClearInjectedErrors();
    if (fail_content_validation)
      ++parameters.pattern_generator_seed;
    TestDownloadHttpResponse::StartServing(parameters, server_url);

    // Download should complete regardless whether content changes or not.
    download->Resume(false);
    WaitForCompletion(download);

    ASSERT_EQ(parameters.size, download->GetReceivedBytes());
    ASSERT_EQ(parameters.size, download->GetTotalBytes());
    ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
        parameters.pattern_generator_seed, parameters.size,
        download->GetTargetFilePath()));

    const TestDownloadResponseHandler::CompletedRequests& requests =
        test_response_handler()->completed_requests();
    ASSERT_EQ(fail_content_validation ? 3u : 2u, requests.size());

    // The first request only transferrs bytes up until the interruption point.
    EXPECT_EQ(interruption_offset, requests[0]->transferred_byte_count);

    // The second request is a range request.
    std::string value;
    ASSERT_FALSE(base::Contains(requests[1]->http_request.headers,
                                net::HttpRequestHeaders::kIfRange));

    ASSERT_TRUE(base::Contains(requests[1]->http_request.headers,
                               net::HttpRequestHeaders::kRange));
    EXPECT_EQ(
        base::StringPrintf("bytes=%" PRId64 "-",
                           interruption_offset - kValidationLength),
        requests[1]->http_request.headers.at(net::HttpRequestHeaders::kRange));
    if (fail_content_validation) {
      // The third request is a restart request.
      ASSERT_FALSE(base::Contains(requests[2]->http_request.headers,
                                  net::HttpRequestHeaders::kRange));
      EXPECT_EQ(parameters.size, requests[2]->transferred_byte_count);
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test fixture for parallel downloading.
class ParallelDownloadTest : public DownloadContentTest {
 public:
  ParallelDownloadTest(const ParallelDownloadTest&) = delete;
  ParallelDownloadTest& operator=(const ParallelDownloadTest&) = delete;

 protected:
  ParallelDownloadTest() {
    std::map<std::string, std::string> params = {
        {download::kMinSliceSizeFinchKey, "1"},
        {download::kParallelRequestCountFinchKey,
         base::NumberToString(kTestRequestCount)},
        {download::kParallelRequestDelayFinchKey, "0"},
        {download::kParallelRequestRemainingTimeFinchKey, "0"}};
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        download::features::kParallelDownloading, params);
  }

  ~ParallelDownloadTest() override {}

  // Creates the intermediate file that has already contained randomly generated
  // download data pieces.
  download::DownloadItem* CreateDownloadAndIntermediateFile(
      const base::FilePath& path,
      const std::vector<GURL>& url_chain,
      const download::DownloadItem::ReceivedSlices& slices,
      const TestDownloadHttpResponse::Parameters& parameters) {
    std::string output;
    int64_t total_bytes = 0u;
    const int64_t kBufferSize = 64 * 1024;
    {
      base::ScopedAllowBlockingForTesting allow_io_for_test_setup;
      base::File file(path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
      for (const auto& slice : slices) {
        EXPECT_TRUE(file.IsValid());
        int64_t length = slice.offset + slice.received_bytes;
        for (int64_t offset = slice.offset; offset < length;) {
          int64_t bytes_to_write =
              length - offset > kBufferSize ? kBufferSize : length - offset;
          output = TestDownloadHttpResponse::GetPatternBytes(
              parameters.pattern_generator_seed, offset, bytes_to_write);
          EXPECT_EQ(
              bytes_to_write,
              UNSAFE_TODO(file.Write(offset, output.data(), bytes_to_write)));
          total_bytes += bytes_to_write;
          offset += bytes_to_write;
        }
      }
      file.Close();
    }

    // Parallel download should create more than 1 slices in most cases. If
    // there is only one slice, consider this is a regular download and remove
    // all slices.
    download::DownloadItem::ReceivedSlices parallel_slices;
    if (slices.size() != 1 || slices[0].offset != 0)
      parallel_slices = slices;
    download::DownloadItem* download =
        DownloadManagerForShell(shell())->CreateDownloadItem(
            "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, path, base::FilePath(),
            url_chain, GURL(),
            StoragePartitionConfig::CreateDefault(
                shell()->web_contents()->GetBrowserContext()),
            GURL(), GURL(), url::Origin(), "application/octet-stream",
            "application/octet-stream", base::Time::Now(), base::Time(),
            parameters.etag, parameters.last_modified, total_bytes,
            parameters.size, std::string(), download::DownloadItem::INTERRUPTED,
            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
            base::Time(), false, parallel_slices);
    ClearAutoResumptionCount(download);
    return download;
  }

  // Verifies parallel download resumption in different scenarios, where the
  // intermediate file is generated based on |slices| and has a full length of
  // |total_length|.
  void RunResumptionTest(
      const download::DownloadItem::ReceivedSlices& received_slices,
      int64_t total_length,
      size_t expected_request_count,
      bool support_partial_response) {
    TestDownloadHttpResponse::Parameters parameters;
    parameters.etag = "ABC";
    parameters.size = total_length;
    parameters.last_modified = std::string();
    parameters.support_partial_response = support_partial_response;
    // Needed to specify HTTP connection type to create parallel download.
    parameters.connection_type = net::HttpConnectionInfo::kHTTP1_1;
    RunResumptionTestWithParameters(received_slices, expected_request_count,
                                    parameters);
  }

  // Similar to the above method, but with given http response parameters.
  void RunResumptionTestWithParameters(
      const download::DownloadItem::ReceivedSlices& received_slices,
      size_t expected_request_count,
      const TestDownloadHttpResponse::Parameters& parameters) {
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(download::features::kParallelDownloading));
    GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
    GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
    TestDownloadHttpResponse::StartServing(parameters, server_url);

    base::FilePath intermediate_file_path =
        GetDownloadDirectory().AppendASCII("intermediate");
    std::vector<GURL> url_chain;
    url_chain.push_back(server_url);

    // Create the intermediate file reflecting the received slices.
    download::DownloadItem* download = CreateDownloadAndIntermediateFile(
        intermediate_file_path, url_chain, received_slices, parameters);

    // Resume the parallel download with sparse file and received slices data.
    download->Resume(false);
    WaitForCompletion(download);
    // TODO(qinmin): count the failed partial responses in DownloadJob when
    // support_partial_response is false. EmbeddedTestServer doesn't know
    // whether completing or canceling the response will come first.
    if (parameters.support_partial_response) {
      test_response_handler()->WaitUntilCompletion(expected_request_count);

      // Verify number of requests sent to the server.
      const TestDownloadResponseHandler::CompletedRequests& completed_requests =
          test_response_handler()->completed_requests();
      EXPECT_EQ(expected_request_count, completed_requests.size());
    }

    // Verify download content on disk.
    ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                              parameters.size, download->GetTargetFilePath());
  }

  // Kicks off the verifies parallel download completion
  void RunCompletionTest(TestDownloadHttpResponse::Parameters& parameters) {
    ErrorStreamCountingObserver observer;
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(download::features::kParallelDownloading));

    GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
    GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());

    // Only parallel download needs to specify the connection type to http 1.1,
    // other tests will automatically fall back to non-parallel download even if
    // the ParallelDownloading feature is enabled based on
    // fieldtrial_testing_config.json.
    parameters.connection_type = net::HttpConnectionInfo::kHTTP1_1;
    TestRequestPauseHandler request_pause_handler;
    parameters.on_pause_handler = request_pause_handler.GetOnPauseHandler();
    // Send some data for the first request and pause it so download won't
    // complete before other parallel requests are created.
    parameters.pause_offset = kPauseOffset;
    TestDownloadHttpResponse::StartServing(parameters, server_url);

    download::DownloadItem* download =
        StartDownloadAndReturnItem(shell(), server_url);

    if (parameters.support_partial_response)
      test_response_handler()->WaitUntilCompletion(2u);
    else
      observer.WaitForFinished(download, 2);

    // Now resume the first request.
    request_pause_handler.Resume();
    WaitForCompletion(download);
    if (parameters.support_partial_response) {
      test_response_handler()->WaitUntilCompletion(3u);
      const TestDownloadResponseHandler::CompletedRequests& completed_requests =
          test_response_handler()->completed_requests();
      EXPECT_EQ(3u, completed_requests.size());
    }
    ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                              parameters.size, download->GetTargetFilePath());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class DownloadPrerenderTest : public DownloadContentTest {
 public:
  DownloadPrerenderTest()
      : prerender_helper_(
            base::BindRepeating(&DownloadPrerenderTest::GetWebContents,
                                base::Unretained(this))) {}
  ~DownloadPrerenderTest() override = default;

  void SetUp() override {
    prerender_helper_.RegisterServerRequestMonitor(embedded_test_server());
    DownloadContentTest::SetUp();
  }

  void SetUpOnMainThread() override {
    DownloadContentTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Started());
  }

  test::PrerenderTestHelper* prerender_helper() { return &prerender_helper_; }

 private:
  WebContents* GetWebContents() { return shell()->web_contents(); }

  test::PrerenderTestHelper prerender_helper_;
};

class DownloadFencedFrameTest : public DownloadContentTest {
 public:
  DownloadFencedFrameTest() {
    fenced_frame_helper_ = std::make_unique<test::FencedFrameTestHelper>();
  }

  ~DownloadFencedFrameTest() override = default;

  void SetUpOnMainThread() override {
    DownloadContentTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Started());
  }

 protected:
  RenderFrameHost* CreateFencedFrame(RenderFrameHost* fenced_frame_parent,
                                     const GURL& url) {
    if (fenced_frame_helper_)
      return fenced_frame_helper_->CreateFencedFrame(fenced_frame_parent, url);

    // FencedFrameTestHelper only supports the MPArch version of fenced frames.
    // So need to maually create a fenced frame for the ShadowDOM version.
    constexpr char kAddFencedFrameScript[] = R"({
        const fenced_frame = document.createElement('fencedframe');
        document.body.appendChild(fenced_frame);
    })";
    EXPECT_TRUE(ExecJs(fenced_frame_parent, kAddFencedFrameScript));

    // Navigate the fenced frame from inside itself, just like the
    // `FencedFrameTestHelper` does for MPArch.
    RenderFrameHostImpl* rfh =
        static_cast<RenderFrameHostImpl*>(ChildFrameAt(fenced_frame_parent, 0));
    FrameTreeNode* target_node = rfh->frame_tree_node();
    constexpr char kNavigateInFencedFrameScript[] = R"({
        location.href = $1;
    })";

    TestNavigationManager navigation(shell()->web_contents(), url);
    EXPECT_EQ(url.spec(),
              EvalJs(rfh, JsReplace(kNavigateInFencedFrameScript, url)));
    EXPECT_TRUE(navigation.WaitForNavigationFinished());

    EXPECT_FALSE(target_node->current_frame_host()->IsErrorDocument());
    return target_node->current_frame_host();
  }

 private:
  std::unique_ptr<test::FencedFrameTestHelper> fenced_frame_helper_;
  base::test::ScopedFeatureList feature_list_;
};

}  // namespace

// Flaky. See https://crbug.com/754679.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadCancelled) {
  SetupEnsureNoPendingDownloads();

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   SlowDownloadHttpResponse::kSlowResponseHostName,
                   SlowDownloadHttpResponse::kUnknownSizeUrl));
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download->GetState());

  // Cancel the download and wait for download system quiesce.
  download->Cancel(true);
  DownloadTestFlushObserver flush_observer(DownloadManagerForShell(shell()));
  flush_observer.WaitForFlush();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

// Check that downloading multiple (in this case, 2) files does not result in
// corrupted files.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, MultiDownload) {
  SetupEnsureNoPendingDownloads();

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download1 = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   SlowDownloadHttpResponse::kSlowResponseHostName,
                   SlowDownloadHttpResponse::kUnknownSizeUrl));
  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download1->GetState());

  // Start the second download and wait until it's done.
  download::DownloadItem* download2 = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib"));
  WaitForCompletion(download2);

  ASSERT_EQ(download::DownloadItem::IN_PROGRESS, download1->GetState());
  ASSERT_EQ(download::DownloadItem::COMPLETE, download2->GetState());

  // Allow the first request to finish.
  std::unique_ptr<DownloadTestObserver> observer2(CreateWaiter(shell(), 1));
  EXPECT_TRUE(NavigateToURL(
      shell(), embedded_test_server()->GetURL(
                   SlowDownloadHttpResponse::kSlowResponseHostName,
                   SlowDownloadHttpResponse::kFinishSlowResponseUrl)));

  observer2->WaitForFinished();  // Wait for the third request.

  EXPECT_EQ(
      1u, observer2->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // The |DownloadItem|s should now be done and have the final file names.
  // Verify that the files have the expected data and size.
  // |file1| should be full of '*'s, and |file2| should be the same as the
  // source file.
  base::FilePath file1(download1->GetTargetFilePath());
  size_t file_size1 = SlowDownloadHttpResponse::kFirstResponsePartSize +
                      SlowDownloadHttpResponse::kSecondResponsePartSize;
  std::string expected_contents(file_size1, '*');
  ASSERT_TRUE(VerifyFile(file1, expected_contents, file_size1));

  base::FilePath file2(download2->GetTargetFilePath());
  ASSERT_TRUE(base::ContentsEqual(
      file2, GetTestFilePath("download", "download-test.lib")));
}

// Tests that metrics are recorded when a page opens a named window, navigates
// it to a URL, then navigates it again to a download. The navigated URL is same
// origin as the opener (example.com). The actual download URL doesn't matter.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       InitiatedByWindowOpener_SameOrigin) {
  EXPECT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL(kOrigin, "/empty.html")));

  // From the initial tab, open a window named 'foo' and navigate it to a same
  // origin page.
  const GURL url = embedded_test_server()->GetURL(kOrigin, "/title1.html");
  const std::string script = "window.open('" + url.spec() + "', 'foo')";
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell()->web_contents(), script));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  // From the initial tab, navigate the 'foo' window to a download and wait for
  // completion.
  base::HistogramTester histogram_tester;
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(new_shell, 1));
  const GURL download_url = embedded_test_server()->GetURL(
      kOtherOrigin, "/download/download-test.lib");
  const std::string download_script =
      "window.open('" + download_url.spec() + "', 'foo')";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), download_script));
  observer->WaitForFinished();

  histogram_tester.ExpectTotalCount("Download.InitiatedByWindowOpener", 1);
  histogram_tester.ExpectUniqueSample(
      "Download.InitiatedByWindowOpener",
      static_cast<int>(InitiatedByWindowOpenerType::kSameOrigin), 1);
}

// Same as InitiatedByWindowOpener_SameOrigin, but the navigated URL is same
// site as the opener (example.com vs one.example.com).
IN_PROC_BROWSER_TEST_F(DownloadContentTest, InitiatedByWindowOpener_SameSite) {
  EXPECT_TRUE(
      NavigateToURL(shell()->web_contents(),
                    embedded_test_server()->GetURL(kOrigin, "/empty.html")));

  // From the initial tab, open a window named 'foo' and navigate it to a
  // subdomain. This is cross-origin but same site.
  const GURL url =
      embedded_test_server()->GetURL(kOriginSubdomain, "/title1.html");

  const std::string script = "window.open('" + url.spec() + "', 'foo')";
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell()->web_contents(), script));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  // From the initial tab, navigate the 'foo' window to a download and wait for
  // completion.
  base::HistogramTester histogram_tester;
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(new_shell, 1));
  const GURL download_url = embedded_test_server()->GetURL(
      kOtherOrigin, "/download/download-test.lib");
  const std::string download_script =
      "window.open('" + download_url.spec() + "', 'foo')";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), download_script));
  observer->WaitForFinished();

  histogram_tester.ExpectTotalCount("Download.InitiatedByWindowOpener", 1);
  histogram_tester.ExpectUniqueSample(
      "Download.InitiatedByWindowOpener",
      static_cast<int>(InitiatedByWindowOpenerType::kSameSite), 1);
}

// The opener and the openee are under the same domain name blogspot.com, but
// blogspot.com is a private registry according to the Public Suffix List, so
// its subdomains are not considered same host.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       InitiatedByWindowOpener_PrivateRegistry) {
  EXPECT_TRUE(NavigateToURL(
      shell()->web_contents(),
      embedded_test_server()->GetURL(kBlogspotSite1, "/empty.html")));

  // From the initial tab, open a window named 'foo' and navigate it to another
  // subdomain of blogspot.com.
  const GURL url =
      embedded_test_server()->GetURL(kBlogspotSite2, "/title1.html");

  const std::string script = "window.open('" + url.spec() + "', 'foo')";
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell()->web_contents(), script));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  // From the initial tab, navigate the 'foo' window to a download and wait for
  // completion.
  base::HistogramTester histogram_tester;
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(new_shell, 1));
  const GURL download_url = embedded_test_server()->GetURL(
      kOtherOrigin, "/download/download-test.lib");
  const std::string download_script =
      "window.open('" + download_url.spec() + "', 'foo')";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), download_script));
  observer->WaitForFinished();

  histogram_tester.ExpectTotalCount("Download.InitiatedByWindowOpener", 1);
  histogram_tester.ExpectUniqueSample(
      "Download.InitiatedByWindowOpener",
      static_cast<int>(InitiatedByWindowOpenerType::kCrossOrigin), 1);
}

// Same as InitiatedByWindowOpener_SameOrigin, but the navigated URL is cross
// origin to the opener (example.com vs example.site).
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       InitiatedByWindowOpener_CrossOrigin) {
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/empty.html")));

  // From the initial tab, open a window named 'foo' and navigate it to a cross
  // origin page.
  const GURL url = embedded_test_server()->GetURL(kOtherOrigin, "/title1.html");
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(ExecJs(shell()->web_contents(),
                     "window.open('" + url.spec() + "', 'foo')"));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  // From the initial tab, navigate the 'foo' window to a download and wait for
  // completion.
  base::HistogramTester histogram_tester;
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(new_shell, 1));
  const GURL download_url = embedded_test_server()->GetURL(
      kOtherOrigin, "/download/download-test.lib");
  const std::string download_script =
      "window.open('" + download_url.spec() + "', 'foo')";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), download_script));
  observer->WaitForFinished();

  histogram_tester.ExpectTotalCount("Download.InitiatedByWindowOpener", 1);
  histogram_tester.ExpectUniqueSample(
      "Download.InitiatedByWindowOpener",
      static_cast<int>(InitiatedByWindowOpenerType::kCrossOrigin), 1);
}

// Same as InitiatedByWindowOpener_CrossOrigin, but the newly opened tab is
// about:blank.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       InitiatedByWindowOpener_CrossOrigin_NonHttpOrHttps) {
  EXPECT_TRUE(NavigateToURL(shell()->web_contents(),
                            embedded_test_server()->GetURL("/empty.html")));

  // From the initial tab, open a window named 'foo' and navigate it to
  // about:blank.
  ShellAddedObserver new_shell_observer;
  EXPECT_TRUE(
      ExecJs(shell()->web_contents(), "window.open('about:blank', 'foo')"));
  Shell* new_shell = new_shell_observer.GetShell();
  ASSERT_TRUE(new_shell);
  EXPECT_TRUE(WaitForLoadStop(new_shell->web_contents()));

  // From the initial tab, navigate the 'foo' window to a download and wait for
  // completion.
  base::HistogramTester histogram_tester;
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(new_shell, 1));
  const GURL download_url = embedded_test_server()->GetURL(
      kOtherOrigin, "/download/download-test.lib");
  const std::string download_script =
      "window.open('" + download_url.spec() + "', 'foo')";
  EXPECT_TRUE(ExecJs(shell()->web_contents(), download_script));
  observer->WaitForFinished();

  histogram_tester.ExpectTotalCount("Download.InitiatedByWindowOpener", 1);
  histogram_tester.ExpectUniqueSample(
      "Download.InitiatedByWindowOpener",
      static_cast<int>(InitiatedByWindowOpenerType::kNonHTTPOrHTTPS), 1);
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Content served with a MIME type of application/octet-stream should be
// downloaded even when a plugin can be found that handles the file type.
// See https://crbug.com/104331 for the details.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadOctetStream) {
  const char16_t kTestPluginName[] = u"TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  WebPluginInfo plugin_info;
  plugin_info.name = kTestPluginName;
  plugin_info.mime_types.push_back(
      WebPluginMimeType(kTestMimeType, kTestFileType, ""));
  plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  PluginServiceImpl::GetInstance()->RegisterInternalPlugin(plugin_info, false);

  // The following is served with a Content-Type of application/octet-stream.
  NavigateToURLAndWaitForDownload(
      shell(), embedded_test_server()->GetURL("/download/octet-stream.abc"),
      download::DownloadItem::COMPLETE);
}

// Content served with a MIME type of application/octet-stream should be
// downloaded even when a plugin can be found that handles the file type.
// See https://crbug.com/104331 for the details.
// In this test, the url is in scope of a service worker but the response is
// served from network.
// This is regression test for https://crbug.com/896696.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadOctetStream_PassThroughServiceWorker) {
  const char16_t kTestPluginName[] = u"TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  RegisterServiceWorker(shell(), "/fetch_event_passthrough.js");

  WebPluginInfo plugin_info;
  plugin_info.name = kTestPluginName;
  plugin_info.mime_types.push_back(
      WebPluginMimeType(kTestMimeType, kTestFileType, ""));
  plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  PluginServiceImpl::GetInstance()->RegisterInternalPlugin(plugin_info, false);

  // The following is served with a Content-Type of application/octet-stream.
  NavigateToURLAndWaitForDownload(
      shell(), embedded_test_server()->GetURL("/download/octet-stream.abc"),
      download::DownloadItem::COMPLETE);
}

// Content served with a MIME type of application/octet-stream should be
// downloaded even when a plugin can be found that handles the file type.
// See https://crbug.com/104331 for the details.
// In this test, the response will be served from a service worker.
// This is regression test for https://crbug.com/896696.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadOctetStream_OctetStreamServiceWorker) {
  const char16_t kTestPluginName[] = u"TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  RegisterServiceWorker(shell(), "/fetch_event_octet_stream.js");

  WebPluginInfo plugin_info;
  plugin_info.name = kTestPluginName;
  plugin_info.mime_types.push_back(
      WebPluginMimeType(kTestMimeType, kTestFileType, ""));
  plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  PluginServiceImpl::GetInstance()->RegisterInternalPlugin(plugin_info, false);

  // The following is served with a Content-Type of application/octet-stream.
  NavigateToURLAndWaitForDownload(
      shell(), embedded_test_server()->GetURL("/download/octet-stream.abc"),
      download::DownloadItem::COMPLETE);
}

// Content served with a MIME type of application/octet-stream should be
// downloaded even when a plugin can be found that handles the file type.
// See https://crbug.com/104331 for the details.
// In this test, the url is in scope of a service worker and the response is
// served from the network via service worker.
// This is regression test for https://crbug.com/896696.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadOctetStream_RespondWithFetchServiceWorker) {
  const char16_t kTestPluginName[] = u"TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  RegisterServiceWorker(shell(), "/fetch_event_respond_with_fetch.js");

  WebPluginInfo plugin_info;
  plugin_info.name = kTestPluginName;
  plugin_info.mime_types.push_back(
      WebPluginMimeType(kTestMimeType, kTestFileType, ""));
  plugin_info.type = WebPluginInfo::PLUGIN_TYPE_PEPPER_IN_PROCESS;
  PluginServiceImpl::GetInstance()->RegisterInternalPlugin(plugin_info, false);

  // The following is served with a Content-Type of application/octet-stream.
  NavigateToURLAndWaitForDownload(
      shell(), embedded_test_server()->GetURL("/download/octet-stream.abc"),
      download::DownloadItem::COMPLETE);
}

#endif

// Try to cancel just before we release the download file, by delaying final
// rename callback.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtFinalRename) {
  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));
  download_manager->SetDownloadFileFactoryForTesting(
      std::unique_ptr<download::DownloadFileFactory>(file_factory));

  // Create a download
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib")));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::OnceClosure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  std::move(callbacks[0]).Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Cancel it.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  download_manager->GetAllDownloads(&items);
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  RunAllTasksUntilIdle();

  // Check state.
  EXPECT_EQ(download::DownloadItem::CANCELLED, items[0]->GetState());

  // Run final rename callback.
  std::move(callbacks[0]).Run();
  callbacks.clear();

  // Check state.
  EXPECT_EQ(download::DownloadItem::CANCELLED, items[0]->GetState());
}

// Try to cancel just after we release the download file, by delaying
// in ShouldOpenDownload.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelAtRelease) {
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));

  // Mark delegate for delayed open.
  GetDownloadManagerDelegate()->SetDelayedOpen(true);

  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  download_manager->SetDownloadFileFactoryForTesting(
      std::unique_ptr<download::DownloadFileFactory>(file_factory));

  // Create a download
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib")));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::OnceClosure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  std::move(callbacks[0]).Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  std::move(callbacks[0]).Run();
  callbacks.clear();

  // Confirm download still IN_PROGRESS (internal state COMPLETING).
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Cancel the download; confirm cancel fails.
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Need to complete open test.
  std::vector<DownloadOpenDelayedCallback> delayed_callbacks;
  GetDownloadManagerDelegate()->GetDelayedCallbacks(
      &delayed_callbacks);
  ASSERT_EQ(1u, delayed_callbacks.size());
  std::move(delayed_callbacks[0]).Run(true);

  // *Now* the download should be complete.
  EXPECT_EQ(download::DownloadItem::COMPLETE, items[0]->GetState());
}

// Try to shutdown with a download in progress to make sure shutdown path
// works properly.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownInProgress) {
  // Create a download that won't complete.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   SlowDownloadHttpResponse::kSlowResponseHostName,
                   SlowDownloadHttpResponse::kUnknownSizeUrl));

  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, download->GetState());

  // Shutdown the download manager and make sure we get the right
  // notifications in the right order.
  StrictMock<MockDownloadItemObserver> item_observer;
  download->AddObserver(&item_observer);
  MockDownloadManagerObserver manager_observer(
      DownloadManagerForShell(shell()));
  // Don't care about ModelChanged() events.
  EXPECT_CALL(manager_observer, ModelChanged(_))
      .WillRepeatedly(Return());
  {
    InSequence notifications;

    EXPECT_CALL(manager_observer, MockManagerGoingDown(
        DownloadManagerForShell(shell())))
        .WillOnce(Return());
    EXPECT_CALL(item_observer,
                OnDownloadUpdated(AllOf(
                    download, Property(&download::DownloadItem::GetState,
                                       download::DownloadItem::CANCELLED))))
        .WillOnce(Return());
    EXPECT_CALL(item_observer, OnDownloadDestroyed(download))
        .WillOnce(Return());
  }

  DownloadManagerForShell(shell())->Shutdown();
}

// Try to shutdown just after we release the download file, by delaying
// release.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownAtRelease) {
  DownloadManagerImpl* download_manager(DownloadManagerForShell(shell()));

  // Mark delegate for delayed open.
  GetDownloadManagerDelegate()->SetDelayedOpen(true);

  // Setup new factory.
  DownloadFileWithDelayFactory* file_factory =
      new DownloadFileWithDelayFactory();
  download_manager->SetDownloadFileFactoryForTesting(
      std::unique_ptr<download::DownloadFileFactory>(file_factory));

  // Create a download
  EXPECT_TRUE(NavigateToURLAndExpectNoCommit(
      shell(), embedded_test_server()->GetURL("/download/download-test.lib")));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::OnceClosure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  std::move(callbacks[0]).Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  std::move(callbacks[0]).Run();
  callbacks.clear();

  // Confirm download isn't complete yet.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  DownloadManagerForShell(shell())->GetAllDownloads(&items);
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, items[0]->GetState());

  // Cancel the download; confirm cancel fails anyway.
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, items[0]->GetState());
  RunAllTasksUntilIdle();
  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, items[0]->GetState());

  MockDownloadItemObserver observer;
  items[0]->AddObserver(&observer);
  EXPECT_CALL(observer, OnDownloadDestroyed(items[0].get()));

  // Shutdown the download manager.  Mostly this is confirming a lack of
  // crashes.
  DownloadManagerForShell(shell())->Shutdown();
}

// Test resumption with a response that contains strong validators.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeWithStrongValidators) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  int64_t interruption_offset = parameters.injected_errors.front();
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  ASSERT_EQ(interruption_offset, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  // Characterization risk: The next portion of the test examines the requests
  // that were sent out while downloading our resource. These requests
  // correspond to the requests that were generated by the browser and the
  // downloads system and may change as implementation details change.
  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  ASSERT_EQ(2u, requests.size());

  // The first request only transferrs bytes up until the interruption point.
  EXPECT_EQ(interruption_offset, requests[0]->transferred_byte_count);

  // The next request should only have transferred the remainder of the
  // resource.
  EXPECT_EQ(parameters.size - interruption_offset,
            requests[1]->transferred_byte_count);

  std::string value;
  ASSERT_TRUE(base::Contains(requests[1]->http_request.headers,
                             net::HttpRequestHeaders::kIfRange));
  EXPECT_EQ(parameters.etag, requests[1]->http_request.headers.at(
                                 net::HttpRequestHeaders::kIfRange));

  ASSERT_TRUE(base::Contains(requests[1]->http_request.headers,
                             net::HttpRequestHeaders::kRange));
  EXPECT_EQ(
      base::StringPrintf("bytes=%" PRId64 "-", interruption_offset),
      requests[1]->http_request.headers.at(net::HttpRequestHeaders::kRange));
}

// Test resumption when strong validators are not present in the response.
IN_PROC_BROWSER_TEST_F(DownloadContentTestWithoutStrongValidators,
                       ResumeWithoutStrongValidators) {
  InterruptAndResumeDownloadWithoutStrongValidators(false);
}

// Test resumption when strong validators are not present in the response and
// the content of the download changes.
IN_PROC_BROWSER_TEST_F(DownloadContentTestWithoutStrongValidators,
                       ResumeWithoutStrongValidatorsAndFailValidation) {
  InterruptAndResumeDownloadWithoutStrongValidators(true);
}

// Resumption should only attempt to contact the final URL if the download has a
// URL chain.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RedirectBeforeResume) {
  SetupErrorInjectionDownloads();
  GURL first_url = embedded_test_server()->GetURL("example.com", "/first-url");
  GURL second_url =
      embedded_test_server()->GetURL("example.com", "/second-url");
  GURL third_url = embedded_test_server()->GetURL("example.com", "/third-url");
  GURL download_url =
      embedded_test_server()->GetURL("example.com", "/download");
  TestDownloadHttpResponse::StartServingStaticResponse(
      base::StringPrintf("HTTP/1.1 302 Redirect\r\n"
                         "Location: %s\r\n\r\n",
                         second_url.spec().c_str()),
      first_url);

  TestDownloadHttpResponse::StartServingStaticResponse(
      base::StringPrintf("HTTP/1.1 302 Redirect\r\n"
                         "Location: %s\r\n\r\n",
                         third_url.spec().c_str()),
      second_url);

  TestDownloadHttpResponse::StartServingStaticResponse(
      base::StringPrintf("HTTP/1.1 302 Redirect\r\n"
                         "Location: %s\r\n\r\n",
                         download_url.spec().c_str()),
      third_url);

  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, download_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), first_url);
  WaitForInterrupt(download);

  EXPECT_EQ(4u, download->GetUrlChain().size());
  EXPECT_EQ(first_url, download->GetOriginalUrl());
  EXPECT_EQ(download_url, download->GetURL());

  // Now that the download is interrupted, make all intermediate servers return
  // a 404. The only way a resumption request would succeed if the resumption
  // request is sent to the final server in the chain.
  TestDownloadHttpResponse::StartServingStaticResponse(k404Response, first_url);
  TestDownloadHttpResponse::StartServingStaticResponse(k404Response,
                                                       second_url);
  TestDownloadHttpResponse::StartServingStaticResponse(k404Response, third_url);

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, download_url);

  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));
}

// If a resumption request results in a redirect, the response should be ignored
// and the download should be marked as interrupted again.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RedirectWhileResume) {
  SetupErrorInjectionDownloads();
  GURL first_url = embedded_test_server()->GetURL("example.com", "/first-url");
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  ++parameters.pattern_generator_seed;
  TestDownloadHttpResponse::StartServing(parameters, first_url);

  // We should never send a request to the decoy. If we do, the request will
  // always succeed, which results in behavior that diverges from what we want,
  // which is for the download to return to being interrupted.
  GURL second_url = embedded_test_server()->GetURL("example.com", "/decoy");
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         second_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), first_url);
  WaitForInterrupt(download);

  // Upon resumption, the server starts responding with a redirect. This
  // response should not be accepted.
  TestDownloadHttpResponse::StartServingStaticResponse(
      base::StringPrintf("HTTP/1.1 302 Redirect\r\n"
                         "Location: %s\r\n\r\n",
                         second_url.spec().c_str()),
      first_url);
  download->Resume(false);
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
            download->GetLastReason());

  // Back to the original request handler. Resumption should now succeed, and
  // use the partial data it had prior to the first interruption.
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, first_url);
  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  // Characterization risk: The next portion of the test examines the requests
  // that were sent out while downloading our resource. These requests
  // correspond to the requests that were generated by the browser and the
  // downloads system and may change as implementation details change.
  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  ASSERT_EQ(3u, requests.size());

  // None of the request should have transferred the entire resource. The
  // redirect response shows up as a response with 0 bytes transferred.
  EXPECT_GT(parameters.size, requests[0]->transferred_byte_count);
  EXPECT_EQ(0, requests[1]->transferred_byte_count);
  EXPECT_GT(parameters.size, requests[2]->transferred_byte_count);
}

// Verify that DownloadUrl can support URL redirect.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RedirectDownload) {
  // Setup a redirect chain with two URL.
  GURL first_url = embedded_test_server()->GetURL("example.com", "/first-url");
  GURL download_url =
      embedded_test_server()->GetURL("example.com", "/download");
  TestDownloadHttpResponse::StartServingStaticResponse(
      base::StringPrintf("HTTP/1.1 302 Redirect\r\n"
                         "Location: %s\r\n\r\n",
                         download_url.spec().c_str()),
      first_url);
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         download_url);

  // Start a download and explicitly specify to support redirect.
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      first_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  download_parameters->set_cross_origin_redirects(
      network::mojom::RedirectMode::kFollow);
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Verify download failed.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());
}

// Verify that DownloadUrl can detect and fail a cross-origin URL redirect.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, FailCrossOriginDownload) {
  // Setup a cross-origin redirect chain with two URLs.
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());

  GURL first_url = origin_one.GetURL("/first-url");
  GURL second_url = origin_two.GetURL("/download");

  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/first-url", second_url));
  origin_one.StartAcceptingConnections();
  origin_two.StartAcceptingConnections();

  // Start a download and explicitly specify to fail cross-origin redirect.
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      first_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  download_parameters->set_cross_origin_redirects(
      network::mojom::RedirectMode::kError);
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Verify download is done.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::INTERRUPTED, downloads[0]->GetState());

  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
}

// Verify that DownloadUrl() to URL with unsafe scheme should fail.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RedirectUnsafeDownload) {
  // Setup a redirect chain with two URL.
  GURL first_url = embedded_test_server()->GetURL("example.com", "/first-url");
  GURL unsafe_url = GURL("unsafe:///etc/passwd");
  TestDownloadHttpResponse::StartServingStaticResponse(
      base::StringPrintf("HTTP/1.1 302 Redirect\r\n"
                         "Location: %s\r\n\r\n",
                         unsafe_url.spec().c_str()),
      first_url);
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         unsafe_url);

  // Start a download and explicitly specify to support redirect.
  DownloadManager* download_manager = DownloadManagerForShell(shell());
  std::unique_ptr<DownloadTestObserverInterrupted> observer =
      std::make_unique<DownloadTestObserverInterrupted>(
          download_manager, 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      first_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  download_parameters->set_cross_origin_redirects(
      network::mojom::RedirectMode::kFollow);
  download_manager->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Verify download failed.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::INTERRUPTED, downloads[0]->GetState());

  // The interrupt reason must match, notice the embedded test server used in
  // tests may also fail even if the download passed the security check.
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_NETWORK_INVALID_REQUEST,
            downloads[0]->GetLastReason());
}

// Verify that DownloadUrl() with no DownloadManagerDelegate drops the download.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, NoDownloadManagerDelegateDownload) {
  const GURL download_url =
      embedded_test_server()->GetURL("/download/download-test.lib");

  // Unset the DownloadManagerDelegate.
  auto* download_manager = DownloadManagerForShell(shell());
  download_manager->GetDelegate()->Shutdown();
  download_manager->SetDelegate(nullptr);

  MockDownloadManagerObserver dm_observer(download_manager);
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);
  EXPECT_CALL(dm_observer, OnDownloadDropped(_)).Times(1);

  // Create download parameters with renderer process information. This is
  // required to go through the DownloadManagerDelegate code path.
  auto download_parameters =
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), download_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  download_parameters->set_content_initiated(true);
  download_manager->DownloadUrl(std::move(download_parameters));

  // Verify there were no downloads.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  EXPECT_TRUE(downloads.empty());
}

// If the server response for the resumption request specifies a bad range (i.e.
// not the range that was requested), then the download should be marked as
// interrupted and restart from the beginning.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, BadRangeHeader) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  // Upon resumption, the server starts responding with a bad range header.
  parameters.ClearInjectedErrors();
  parameters.SetResponseForRangeRequest(
      10000, -1,
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes 1000000-2000000/3000000\r\n"
      "\r\n");
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForCompletion(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
            download->GetLastReason());
}

// If the server response for the resumption request specifies an invalid range,
// then the download should be marked as interrupted and as interrupted again
// without discarding the partial state.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, InvalidRangeHeader) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  // Or this time, the server sends a response with an invalid Content-Range
  // header.
  TestDownloadHttpResponse::StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: ooga-booga-booga-booga\r\n"
      "\r\n",
      server_url);
  download->Resume(false);
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Or no Content-Range header at all.
  TestDownloadHttpResponse::StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Some-Headers: ooga-booga-booga-booga\r\n"
      "\r\n",
      server_url);
  download->Resume(false);
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Back to the original request handler. Resumption should now succeed, and
  // use the partial data it had prior to the first interruption.
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);
  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  // Characterization risk: The next portion of the test examines the requests
  // that were sent out while downloading our resource. These requests
  // correspond to the requests that were generated by the browser and the
  // downloads system and may change as implementation details change.
  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  ASSERT_EQ(4u, requests.size());

  // The last request will transfer the entire resource as the interrupt
  // reason doesn't allow download to continue.
  EXPECT_GT(parameters.size, requests[0]->transferred_byte_count);
  EXPECT_EQ(0, requests[1]->transferred_byte_count);
  EXPECT_EQ(0, requests[2]->transferred_byte_count);
  EXPECT_EQ(parameters.size, requests[3]->transferred_byte_count);
}

// If the server response for the resumption request cannot be decoded,
// the download will need to restart. This is to simulate some servers
// that doesn't handle range request properly.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, BadEncoding) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  parameters.ClearInjectedErrors();
  // Server's response to range request cannot be decoded.
  parameters.SetResponseForRangeRequest(
      10000, -1,
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes 1000000-2000000/3000000\r\n"
      "Content-Encoding: gzip\r\n"
      "\r\n"
      "x\r\n");
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  // The download will restart and complete successfully.
  download->Resume(false);
  WaitForCompletion(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_NO_RANGE,
            download->GetLastReason());
}

// A partial resumption results in an HTTP 200 response. I.e. the server ignored
// the range request and sent the entire resource instead. For If-Range requests
// (as opposed to If-Match), the behavior for a precondition failure is also to
// respond with a 200. So this test case covers both validation failure and
// ignoring the range request.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RestartIfNotPartialResponse) {
  SetupErrorInjectionDownloads();
  const int kOriginalPatternGeneratorSeed = 1;
  const int kNewPatternGeneratorSeed = 2;

  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  parameters.pattern_generator_seed = kOriginalPatternGeneratorSeed;
  int64_t interruption_offset = parameters.injected_errors.front();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  ASSERT_EQ(interruption_offset, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());

  parameters = TestDownloadHttpResponse::Parameters();
  parameters.support_byte_ranges = false;
  parameters.pattern_generator_seed = kNewPatternGeneratorSeed;
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(interruption_offset, download->GetBytesWasted());
  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(
      ReadAndVerifyFileContents(kNewPatternGeneratorSeed, parameters.size,
                                download->GetTargetFilePath()));

  // When the downloads system sees the full response, it should accept the
  // response without restarting. On the network, we should deterministically
  // see two requests:
  // * The original request which transfers upto our interruption point.
  // * The resumption attempt, which receives the entire entity.
  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  ASSERT_EQ(2u, requests.size());

  // The first request only transfers data up to the interruption point.
  EXPECT_EQ(interruption_offset, requests[0]->transferred_byte_count);

  // The second request transfers the entire response.
  EXPECT_EQ(parameters.size, requests[1]->transferred_byte_count);

  ASSERT_TRUE(base::Contains(requests[1]->http_request.headers,
                             net::HttpRequestHeaders::kIfRange));
  EXPECT_EQ(parameters.etag, requests[1]->http_request.headers.at(
                                 net::HttpRequestHeaders::kIfRange));

  ASSERT_TRUE(base::Contains(requests[1]->http_request.headers,
                             net::HttpRequestHeaders::kRange));
  EXPECT_EQ(
      base::StringPrintf("bytes=%" PRId64 "-", interruption_offset),
      requests[1]->http_request.headers.at(net::HttpRequestHeaders::kRange));
}

// Confirm we restart if we don't have a verifier.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RestartIfNoETag) {
  SetupErrorInjectionDownloads();
  const int kOriginalPatternGeneratorSeed = 1;
  const int kNewPatternGeneratorSeed = 2;

  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  ASSERT_EQ(1u, parameters.injected_errors.size());
  parameters.etag.clear();
  parameters.pattern_generator_seed = kOriginalPatternGeneratorSeed;

  TestDownloadHttpResponse::StartServing(parameters, server_url);
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  parameters.pattern_generator_seed = kNewPatternGeneratorSeed;
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(
      ReadAndVerifyFileContents(kNewPatternGeneratorSeed, parameters.size,
                                download->GetTargetFilePath()));

  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  // Neither If-Range nor Range headers should be present in the second request.
  ASSERT_EQ(2u, requests.size());
  EXPECT_FALSE(base::Contains(requests[1]->http_request.headers,
                              net::HttpRequestHeaders::kIfRange));
  EXPECT_FALSE(base::Contains(requests[1]->http_request.headers,
                              net::HttpRequestHeaders::kRange));
}

// Partial file goes missing before the download is resumed. The download should
// restart.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, RestartIfNoPartialFile) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  int64_t interruption_offset = parameters.injected_errors.front();

  TestDownloadHttpResponse::StartServing(parameters, server_url);
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  // Delete the intermediate file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(PathExists(download->GetFullPath()));
    ASSERT_TRUE(base::DeleteFile(download->GetFullPath()));
  }

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(interruption_offset, download->GetBytesWasted());
  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RecoverFromInitFileError) {
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         server_url);

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  const TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_INITIALIZE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE};
  injector->InjectError(err);

  // Start and watch for interrupt.
  download::DownloadItem* download(
      StartDownloadAndReturnItem(shell(), server_url));
  WaitForInterrupt(download);
  ASSERT_EQ(download::DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
            download->GetLastReason());
  EXPECT_EQ(0, download->GetReceivedBytes());
  EXPECT_TRUE(download->GetFullPath().empty());
  EXPECT_FALSE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory.
  RunAllTasksUntilIdle();

  // Clear the old errors list.
  injector->ClearError();

  // Resume and watch completion.
  download->Resume(false);
  WaitForCompletion(download);
  EXPECT_EQ(download->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       RecoverFromIntermediateFileRenameError) {
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         server_url);

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  const TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_RENAME_UNIQUIFY, 0,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE};
  injector->InjectError(err);

  // Start and watch for interrupt.
  download::DownloadItem* download(
      StartDownloadAndReturnItem(shell(), server_url));
  WaitForInterrupt(download);
  ASSERT_EQ(download::DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE,
            download->GetLastReason());
  EXPECT_TRUE(download->GetFullPath().empty());
  // Target path will have been set after file name determination. GetFullPath()
  // being empty is sufficient to signal that filename determination needs to be
  // redone.
  EXPECT_FALSE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory.
  RunAllTasksUntilIdle();

  // Clear the old errors list.
  injector->ClearError();

  download->Resume(false);
  WaitForCompletion(download);
  EXPECT_EQ(download->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RecoverFromFinalRenameError) {
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         server_url);

  // Setup the error injector.
  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_RENAME_ANNOTATE, 0,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED};
  injector->InjectError(err);

  // Start and watch for interrupt.
  download::DownloadItem* download(
      StartDownloadAndReturnItem(shell(), server_url));
  WaitForInterrupt(download);
  ASSERT_EQ(download::DownloadItem::INTERRUPTED, download->GetState());
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_FILE_FAILED,
            download->GetLastReason());
  EXPECT_TRUE(download->GetFullPath().empty());
  // Target path should still be intact.
  EXPECT_FALSE(download->GetTargetFilePath().empty());

  // We need to make sure that any cross-thread downloads communication has
  // quiesced before clearing and injecting the new errors, as the
  // InjectErrors() routine alters the currently in use download file
  // factory, which is a download sequence object.
  RunAllTasksUntilIdle();

  // Clear the old errors list.
  injector->ClearError();

  download->Resume(false);
  WaitForCompletion(download);
  EXPECT_EQ(download->GetState(), download::DownloadItem::COMPLETE);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, Resume_Hash) {
  const char kExpectedHash[] =
      "\xa7\x44\x49\x86\x24\xc6\x84\x6c\x89\xdf\xd8\xec\xa0\xe0\x61\x12\xdc\x80"
      "\x13\xf2\x83\x49\xa9\x14\x52\x32\xf0\x95\x20\xca\x5b\x30";
  std::string expected_hash(kExpectedHash);
  TestDownloadHttpResponse::Parameters parameters;

  // As a control, let's try GetHash() on an uninterrupted download.
  GURL url1 = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url1 = embedded_test_server()->GetURL(url1.host(), url1.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url1);
  download::DownloadItem* uninterrupted_download(
      StartDownloadAndReturnItem(shell(), server_url1));
  WaitForCompletion(uninterrupted_download);
  EXPECT_EQ(expected_hash, uninterrupted_download->GetHash());

  SetupErrorInjectionDownloads();
  // Now with interruptions.
  GURL url2 = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url2 = embedded_test_server()->GetURL(url2.host(), url2.path());
  parameters.inject_error_cb = inject_error_callback();
  parameters.injected_errors.push(100);
  parameters.injected_errors.push(211);
  parameters.injected_errors.push(337);
  parameters.injected_errors.push(400);
  parameters.injected_errors.push(512);
  TestDownloadHttpResponse::StartServing(parameters, server_url2);

  // Start and watch for interrupt.
  download::DownloadItem* download(
      StartDownloadAndReturnItem(shell(), server_url2));
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume(true);
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume(true);
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume(true);
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume(true);
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume(true);
  WaitForCompletion(download);

  EXPECT_EQ(expected_hash, download->GetHash());
}

// An interrupted download should remove the intermediate file when it is
// cancelled.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelInterruptedDownload) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback()),
      server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  base::FilePath intermediate_path = download->GetFullPath();
  ASSERT_FALSE(intermediate_path.empty());
  ASSERT_TRUE(PathExists(intermediate_path));

  download->Cancel(true /* user_cancel */);
  RunAllTasksUntilIdle();

  // The intermediate file should now be gone.
  EXPECT_FALSE(PathExists(intermediate_path));
  EXPECT_TRUE(download->GetFullPath().empty());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveInterruptedDownload) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback()),
      server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  base::FilePath intermediate_path = download->GetFullPath();
  ASSERT_FALSE(intermediate_path.empty());
  ASSERT_TRUE(PathExists(intermediate_path));

  download->Remove();
  RunAllTasksUntilIdle();

  // The intermediate file should now be gone.
  EXPECT_FALSE(PathExists(intermediate_path));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveCompletedDownload) {
  // A completed download shouldn't delete the downloaded file when it is
  // removed.
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(TestDownloadHttpResponse::Parameters(),
                                         server_url);

  std::unique_ptr<DownloadTestObserver> completion_observer(
      CreateWaiter(shell(), 1));
  download::DownloadItem* download(
      StartDownloadAndReturnItem(shell(), server_url));
  completion_observer->WaitForFinished();

  // The target path should exist.
  base::FilePath target_path(download->GetTargetFilePath());
  EXPECT_TRUE(PathExists(target_path));
  download->Remove();
  RunAllTasksUntilIdle();

  // The file should still exist.
  EXPECT_TRUE(PathExists(target_path));
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveResumingDownload) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(PathExists(intermediate_path));

  // Resume and remove download. We expect only a single OnDownloadCreated()
  // call, and that's for the second download created below.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(1);

  TestRequestPauseHandler request_pause_handler;
  parameters.on_pause_handler = request_pause_handler.GetOnPauseHandler();
  parameters.pause_offset = -1;
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  request_pause_handler.WaitForCallback();

  // At this point, the download resumption request has been sent out, but the
  // response hasn't been received yet.
  download->Remove();
  request_pause_handler.Resume();

  // The intermediate file should now be gone.
  RunAllTasksUntilIdle();
  EXPECT_FALSE(PathExists(intermediate_path));

  parameters.ClearInjectedErrors();
  parameters.on_pause_handler.Reset();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  // Start the second download and wait until it's done. This exercises the
  // entire downloads stack and effectively flushes all of our worker threads.
  // We are testing whether the URL request created in the previous
  // download::DownloadItem::Resume() call reulted in a new download or not.
  NavigateToURLAndWaitForDownload(shell(), server_url,
                                  download::DownloadItem::COMPLETE);
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, CancelResumingDownload) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(PathExists(intermediate_path));

  // Resume and cancel download. We expect only a single OnDownloadCreated()
  // call, and that's for the second download created below.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(1);

  TestRequestPauseHandler request_pause_handler;
  parameters.on_pause_handler = request_pause_handler.GetOnPauseHandler();
  parameters.pause_offset = -1;
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  request_pause_handler.WaitForCallback();

  // At this point, the download item has initiated a network request for the
  // resumption attempt, but hasn't received a response yet.
  download->Cancel(true /* user_cancel */);

  request_pause_handler.Resume();

  // The intermediate file should now be gone.
  RunAllPendingInMessageLoop(BrowserThread::IO);
  RunAllTasksUntilIdle();
  EXPECT_FALSE(PathExists(intermediate_path));

  parameters.ClearInjectedErrors();
  parameters.on_pause_handler.Reset();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  // Start the second download and wait until it's done. This exercises the
  // entire downloads stack and effectively flushes all of our worker threads.
  // We are testing whether the URL request created in the previous
  // download::DownloadItem::Resume() call reulted in a new download or not.
  NavigateToURLAndWaitForDownload(shell(), server_url,
                                  download::DownloadItem::COMPLETE);
  EXPECT_TRUE(EnsureNoPendingDownloads());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, RemoveResumedDownload) {
  SetupErrorInjectionDownloads();
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  base::FilePath target_path(download->GetTargetFilePath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(PathExists(intermediate_path));
  EXPECT_FALSE(PathExists(target_path));

  // Resume and remove download. We don't expect OnDownloadCreated() calls.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForInProgress(download);

  download->Remove();

  // The intermediate file should now be gone.
  RunAllTasksUntilIdle();
  EXPECT_FALSE(PathExists(intermediate_path));
  EXPECT_FALSE(PathExists(target_path));
  EXPECT_TRUE(EnsureNoPendingDownloads());
  test_response_handler()->WaitUntilCompletion(2u);
}

// TODO(qinmin): Flaky crashes on ASAN Linux. https://crbug.com/836689
#if BUILDFLAG(IS_ANDROID) ||                           \
    (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
        defined(ADDRESS_SANITIZER)
#define MAYBE_CancelResumedDownload DISABLED_CancelResumedDownload
#else
#define MAYBE_CancelResumedDownload CancelResumedDownload
#endif
IN_PROC_BROWSER_TEST_F(DownloadContentTest, MAYBE_CancelResumedDownload) {
  SetupErrorInjectionDownloads();
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);
  WaitForInterrupt(download);

  base::FilePath intermediate_path(download->GetFullPath());
  base::FilePath target_path(download->GetTargetFilePath());
  ASSERT_FALSE(intermediate_path.empty());
  EXPECT_TRUE(PathExists(intermediate_path));
  EXPECT_FALSE(PathExists(target_path));

  // Resume and remove download. We don't expect OnDownloadCreated() calls.
  MockDownloadManagerObserver dm_observer(DownloadManagerForShell(shell()));
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForInProgress(download);

  download->Cancel(true);

  // The intermediate file should now be gone.
  RunAllTasksUntilIdle();
  EXPECT_FALSE(PathExists(intermediate_path));
  EXPECT_FALSE(PathExists(target_path));
  EXPECT_TRUE(EnsureNoPendingDownloads());
  test_response_handler()->WaitUntilCompletion(2u);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_NoFile) {
  TestDownloadHttpResponse::Parameters parameters;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          parameters.etag, std::string(), kIntermediateSize, parameters.size,
          std::string(), download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  // There will be two requests. The first one is issued optimistically assuming
  // that the intermediate file exists and matches the size expectations set
  // forth in the download metadata (i.e. assuming that a 1331 byte file exists
  // at |intermediate_file_path|.
  //
  // However, once the response is received, DownloadFile will report that the
  // intermediate file doesn't exist and hence the download is marked
  // interrupted again.
  //
  // The second request reads the entire entity.
  //
  // N.b. we can't make any assumptions about how many bytes are transferred by
  // the first request since response data will be bufferred until DownloadFile
  // is done initializing.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(2u, requests.size());
  EXPECT_EQ(parameters.size, requests[1]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_NoHash) {
  TestDownloadHttpResponse::Parameters parameters;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::string output = TestDownloadHttpResponse::GetPatternBytes(
      parameters.pattern_generator_seed, 0, kIntermediateSize);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(intermediate_file_path, output));
  }

  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          parameters.etag, std::string(), kIntermediateSize, parameters.size,
          std::string(), download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();

  // There's only one network request issued, and that is for the remainder of
  // the file.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size - kIntermediateSize,
            completed_requests[0]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeRestoredDownload_EtagMismatch) {
  TestDownloadHttpResponse::Parameters parameters;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::string output = TestDownloadHttpResponse::GetPatternBytes(
      parameters.pattern_generator_seed + 1, 0, kIntermediateSize);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(intermediate_file_path, output));
  }

  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          "fake-etag", std::string(), kIntermediateSize, parameters.size,
          std::string(), download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  EXPECT_EQ(kIntermediateSize, download->GetBytesWasted());
  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();

  // There's only one network request issued. The If-Range header allows the
  // server to respond with the entire entity in one go. The existing contents
  // of the file should be discarded, and overwritten by the new contents.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[0]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       ResumeRestoredDownload_CorrectHash) {
  TestDownloadHttpResponse::Parameters parameters;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::string output = TestDownloadHttpResponse::GetPatternBytes(
      parameters.pattern_generator_seed, 0, kIntermediateSize);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(intermediate_file_path, output));
  }
  // SHA-256 hash of the pattern bytes in buffer.
  static const uint8_t kPartialHash[] = {
      0x77, 0x14, 0xfd, 0x83, 0x06, 0x15, 0x10, 0x7a, 0x47, 0x15, 0xd3,
      0xcf, 0xdd, 0x46, 0xa2, 0x61, 0x96, 0xff, 0xc3, 0xbb, 0x49, 0x30,
      0xaf, 0x31, 0x3a, 0x64, 0x0b, 0xd5, 0xfa, 0xb1, 0xe3, 0x81};

  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          parameters.etag, std::string(), kIntermediateSize, parameters.size,
          std::string(std::begin(kPartialHash), std::end(kPartialHash)),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();

  // There's only one network request issued, and that is for the remainder of
  // the file.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size - kIntermediateSize,
            completed_requests[0]->transferred_byte_count);

  // SHA-256 hash of the entire 102400 bytes in the target file.
  static const uint8_t kFullHash[] = {
      0xa7, 0x44, 0x49, 0x86, 0x24, 0xc6, 0x84, 0x6c, 0x89, 0xdf, 0xd8,
      0xec, 0xa0, 0xe0, 0x61, 0x12, 0xdc, 0x80, 0x13, 0xf2, 0x83, 0x49,
      0xa9, 0x14, 0x52, 0x32, 0xf0, 0x95, 0x20, 0xca, 0x5b, 0x30};
  EXPECT_EQ(std::string(std::begin(kFullHash), std::end(kFullHash)),
            download->GetHash());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_WrongHash) {
  TestDownloadHttpResponse::Parameters parameters;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  std::vector<char> buffer(kIntermediateSize);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(intermediate_file_path,
                                {buffer.data(), buffer.size()}));
  }
  // SHA-256 hash of the expected pattern bytes in buffer. This doesn't match
  // the current contents of the intermediate file which should all be 0.
  static const uint8_t kPartialHash[] = {
      0x77, 0x14, 0xfd, 0x83, 0x06, 0x15, 0x10, 0x7a, 0x47, 0x15, 0xd3,
      0xcf, 0xdd, 0x46, 0xa2, 0x61, 0x96, 0xff, 0xc3, 0xbb, 0x49, 0x30,
      0xaf, 0x31, 0x3a, 0x64, 0x0b, 0xd5, 0xfa, 0xb1, 0xe3, 0x81};

  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          parameters.etag, std::string(), kIntermediateSize, parameters.size,
          std::string(std::begin(kPartialHash), std::end(kPartialHash)),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();

  // There will be two requests. The first one is issued optimistically assuming
  // that the intermediate file exists and matches the size expectations set
  // forth in the download metadata (i.e. assuming that a 1331 byte file exists
  // at |intermediate_file_path|.
  //
  // However, once the response is received, DownloadFile will report that the
  // intermediate file doesn't match the expected hash.
  //
  // The second request reads the entire entity.
  //
  // N.b. we can't make any assumptions about how many bytes are transferred by
  // the first request since response data will be bufferred until DownloadFile
  // is done initializing.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(2u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[1]->transferred_byte_count);

  // SHA-256 hash of the entire 102400 bytes in the target file.
  static const uint8_t kFullHash[] = {
      0xa7, 0x44, 0x49, 0x86, 0x24, 0xc6, 0x84, 0x6c, 0x89, 0xdf, 0xd8,
      0xec, 0xa0, 0xe0, 0x61, 0x12, 0xdc, 0x80, 0x13, 0xf2, 0x83, 0x49,
      0xa9, 0x14, 0x52, 0x32, 0xf0, 0x95, 0x20, 0xca, 0x5b, 0x30};
  EXPECT_EQ(std::string(std::begin(kFullHash), std::end(kFullHash)),
            download->GetHash());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_ShortFile) {
  TestDownloadHttpResponse::Parameters parameters;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  const int kIntermediateSize = 1331;
  // Size of file is slightly shorter than the size known to
  // download::DownloadItem.
  std::string output = TestDownloadHttpResponse::GetPatternBytes(
      parameters.pattern_generator_seed, 0, kIntermediateSize - 100);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(intermediate_file_path, output));
  }
  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          parameters.etag, std::string(), kIntermediateSize, parameters.size,
          std::string(), download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();

  // There will be two requests. The first one is issued optimistically assuming
  // that the intermediate file exists and matches the size expectations set
  // forth in the download metadata (i.e. assuming that a 1331 byte file exists
  // at |intermediate_file_path|.
  //
  // However, once the response is received, DownloadFile will report that the
  // intermediate file is too short and hence the download is marked interrupted
  // again.
  //
  // The second request reads the entire entity.
  //
  // N.b. we can't make any assumptions about how many bytes are transferred by
  // the first request since response data will be bufferred until DownloadFile
  // is done initializing.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(2u, completed_requests.size());
  EXPECT_EQ(parameters.size, completed_requests[1]->transferred_byte_count);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, ResumeRestoredDownload_LongFile) {
  // These numbers are sufficiently large that the intermediate file won't be
  // read in a single Read().
  const int kFileSize = 1024 * 1024;
  const int kIntermediateSize = kFileSize / 2 + 111;

  TestDownloadHttpResponse::Parameters parameters;
  parameters.size = kFileSize;
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  base::FilePath intermediate_file_path =
      GetDownloadDirectory().AppendASCII("intermediate");
  std::vector<GURL> url_chain;

  // Size of file is slightly longer than the size known to
  // download::DownloadItem.
  std::string output = TestDownloadHttpResponse::GetPatternBytes(
      parameters.pattern_generator_seed, 0, kIntermediateSize + 100);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(base::WriteFile(intermediate_file_path, output));
  }
  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(),
          StoragePartitionConfig::CreateDefault(
              shell()->web_contents()->GetBrowserContext()),
          GURL(), GURL(), url::Origin(), "application/octet-stream",
          "application/octet-stream", base::Time::Now(), base::Time(),
          parameters.etag, std::string(), kIntermediateSize, parameters.size,
          std::string(), download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());
  ClearAutoResumptionCount(download);

  download->Resume(false);
  WaitForCompletion(download);

  // The amount "extra" that was added to the file.
  EXPECT_EQ(100, download->GetBytesWasted());
  EXPECT_FALSE(PathExists(intermediate_file_path));
  ReadAndVerifyFileContents(parameters.pattern_generator_seed,
                            parameters.size,
                            download->GetTargetFilePath());

  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();

  // There should be only one request. The intermediate file should be truncated
  // to the expected size, and the request should be issued for the remainder.
  //
  // TODO(asanka): Ideally we'll check that the intermediate file matches
  // expectations prior to issuing the first resumption request.
  ASSERT_EQ(1u, completed_requests.size());
  EXPECT_EQ(parameters.size - kIntermediateSize,
            completed_requests[0]->transferred_byte_count);
}

// Test that the referrer header is set correctly for a download that's resumed
// partially.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ReferrerForPartialResumption) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  GURL document_url = embedded_test_server()->GetURL(
      std::string("/download/download-link.html?dl=")
          .append(server_url.spec()));

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), document_url);
  WaitForInterrupt(download);

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume(false);
  WaitForCompletion(download);

  ASSERT_EQ(parameters.size, download->GetReceivedBytes());
  ASSERT_EQ(parameters.size, download->GetTotalBytes());
  ASSERT_NO_FATAL_FAILURE(ReadAndVerifyFileContents(
      parameters.pattern_generator_seed, parameters.size,
      download->GetTargetFilePath()));

  const TestDownloadResponseHandler::CompletedRequests& requests =
      test_response_handler()->completed_requests();

  ASSERT_GE(2u, requests.size());
  net::test_server::HttpRequest last_request = requests.back()->http_request;
  ASSERT_TRUE(
      base::Contains(last_request.headers, net::HttpRequestHeaders::kReferer));
  EXPECT_EQ(last_request.headers.at(net::HttpRequestHeaders::kReferer),
            document_url.DeprecatedGetOriginAsURL().spec());
}

// Test that the referrer header is dropped for HTTP downloads from HTTPS.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ReferrerForHTTPS) {
  net::EmbeddedTestServer https_origin(
      net::EmbeddedTestServer::Type::TYPE_HTTPS);
  net::EmbeddedTestServer http_origin(net::EmbeddedTestServer::Type::TYPE_HTTP);
  https_origin.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  http_origin.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "application/octet-stream", "Hello"));
  ASSERT_TRUE(https_origin.InitializeAndListen());
  ASSERT_TRUE(http_origin.InitializeAndListen());

  GURL download_url = http_origin.GetURL("/download");
  GURL referrer_url = https_origin.GetURL(
      std::string("/download-link.html?dl=") + download_url.spec());

  https_origin.StartAcceptingConnections();
  http_origin.StartAcceptingConnections();

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), referrer_url);
  WaitForCompletion(download);

  ASSERT_EQ(5, download->GetReceivedBytes());
  EXPECT_EQ("", download->GetReferrerUrl().spec());

  ASSERT_TRUE(https_origin.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(http_origin.ShutdownAndWaitUntilComplete());
}

// Check that the site-for-cookies is correctly updated when downloading a file
// that redirects cross site, by verifying that a SameSite cookie can be set
// following a cross-site redirect.
// (It is not enough to redirect across origins with the same host but different
// port numbers, because cookies do not respect ports.)
IN_PROC_BROWSER_TEST_F(DownloadContentTest, UpdateSiteForCookies) {
  net::EmbeddedTestServer site_a;
  net::EmbeddedTestServer site_b;

  base::StringPairs cookie_headers;
  cookie_headers.push_back(std::make_pair(std::string("Set-Cookie"),
                                          std::string("A=lax; SameSite=Lax")));
  cookie_headers.push_back(std::make_pair(
      std::string("Set-Cookie"), std::string("B=strict; SameSite=Strict")));

  // This will request a URL on b.test, which redirects to a url that sets the
  // cookies on a.test.
  site_a.RegisterRequestHandler(CreateBasicResponseHandler(
      "/sets-samesite-cookies", net::HTTP_OK, cookie_headers,
      "application/octet-stream", "abcd"));
  ASSERT_TRUE(site_a.Start());
  site_b.RegisterRequestHandler(
      CreateRedirectHandler("/redirected-download",
                            site_a.GetURL("a.test", "/sets-samesite-cookies")));
  ASSERT_TRUE(site_b.Start());

  // Download the file.
  SetupEnsureNoPendingDownloads();
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(),
          site_b.GetURL("b.test", "/redirected-download"),
          TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());

  // Check that the cookies were correctly set on a.test.
  EXPECT_EQ("A=lax; B=strict",
            content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                                site_a.GetURL("a.test", "/")));
}

// Tests that if `update_first_party_url_on_redirect` is set to false, download
// will not behave like a top-level frame navigation and SameSite=Strict cookies
// will not be set on a redirection.
IN_PROC_BROWSER_TEST_F(
    DownloadContentTest,
    SiteForCookies_DownloadUrl_NotUpdateFirstPartyUrlOnRedirect) {
  net::EmbeddedTestServer site_a;
  net::EmbeddedTestServer site_b;

  base::StringPairs cookie_headers;
  cookie_headers.push_back(std::make_pair(
      std::string("Set-Cookie"), std::string("A=strict; SameSite=Strict")));
  cookie_headers.push_back(std::make_pair(std::string("Set-Cookie"),
                                          std::string("B=lax; SameSite=Lax")));

  // This will request a URL on b.test, which redirects to a url that sets the
  // cookies on a.test.
  site_a.RegisterRequestHandler(CreateBasicResponseHandler(
      "/sets-samesite-cookies", net::HTTP_OK, cookie_headers,
      "application/octet-stream", "abcd"));
  ASSERT_TRUE(site_a.Start());
  site_b.RegisterRequestHandler(
      CreateRedirectHandler("/redirected-download",
                            site_a.GetURL("a.test", "/sets-samesite-cookies")));
  ASSERT_TRUE(site_b.Start());

  // Download the file.
  SetupEnsureNoPendingDownloads();
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(),
          site_b.GetURL("b.test", "/redirected-download"),
          TRAFFIC_ANNOTATION_FOR_TESTS));
  download_parameters->set_update_first_party_url_on_redirect(false);
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());

  // Check that the cookies were not set on a.test.
  EXPECT_EQ("",
            content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                                site_a.GetURL("a.test", "/")));
}

// Verifies that isolation info set in DownloadUrlParameters can be populated.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       SiteForCookies_DownloadUrl_IsolationInfoPopulated) {
  // Setup a server that sets cookie.
  net::EmbeddedTestServer site_a;
  base::StringPairs cookie_headers;
  cookie_headers.push_back(std::make_pair(std::string("Set-Cookie"),
                                          std::string("A=lax; SameSite=Lax")));
  cookie_headers.push_back(std::make_pair(
      std::string("Set-Cookie"), std::string("B=strict; SameSite=Strict")));
  site_a.RegisterRequestHandler(CreateBasicResponseHandler(
      "/sets-samesite-cookies", net::HTTP_OK, cookie_headers,
      "application/octet-stream", "abcd"));
  ASSERT_TRUE(site_a.Start());

  // Download the file.
  SetupEnsureNoPendingDownloads();
  GURL download_url = site_a.GetURL("a.test", "/sets-samesite-cookies");
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), download_url, TRAFFIC_ANNOTATION_FOR_TESTS));

  // Mark this request a third party request, cookie should be blocked.
  net::IsolationInfo isolation_info =
      net::IsolationInfo::CreateForInternalRequest(
          url::Origin::Create(GURL("http://www.example.com")));
  download_parameters->set_isolation_info(isolation_info);

  // Verify the isolation info.
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  ExpectRequestIsolationInfo(download_url, isolation_info,
                             base::BindLambdaForTesting([&]() {
                               DownloadManagerForShell(shell())->DownloadUrl(
                                   std::move(download_parameters));
                               observer->WaitForFinished();
                             }));

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  // Check no cookies are written for URL a.test since it's a third party
  // cookie.
  EXPECT_TRUE(content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                                  download_url)
                  .empty());
}

// A filename suggestion specified via a @download attribute should not be
// effective if the final download URL is in another origin from the original
// download URL.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeCrossOriginRedirect) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. When
  // the page is loaded, a script simulates a click on the anchor, triggering a
  // download of the target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?target=") + download_url.spec());
  GURL final_url = origin_two.GetURL(kOriginTwo, "/download");
  url::Origin final_url_origin = url::Origin::Create(final_url);
  // The IsolationInfo after the cross-site redirect should be the same as
  // if there were a top-level navigation to the final URL.
  net::IsolationInfo expected_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, final_url_origin,
      final_url_origin, net::SiteForCookies::FromOrigin(final_url_origin));

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/download.
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  origin_one.RegisterRequestHandler(CreateRedirectHandler("/ping", final_url));
  origin_one.StartAcceptingConnections();

  origin_two.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "application/octet-stream", "Hello"));
  origin_two.StartAcceptingConnections();

  ExpectRequestIsolationInfo(
      final_url, expected_isolation_info, base::BindLambdaForTesting([&]() {
        NavigateToCommittedURLAndWaitForDownload(
            shell(), referrer_url, download::DownloadItem::COMPLETE);
      }));

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("download"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A filename suggestion specified via a @download attribute should not be
// effective if there are cross origin redirects in the middle of the redirect
// chain.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeSameOriginRedirect) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. When
  // the page is loaded, a script simulates a click on the anchor, triggering a
  // download of the target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?target=") + download_url.spec());
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/pong, and then finally
  // to <origin_one>/download.
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/pong")));
  origin_two.RegisterRequestHandler(
      CreateRedirectHandler("/pong", origin_one.GetURL("/download")));
  origin_one.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "application/octet-stream", "Hello"));

  origin_one.StartAcceptingConnections();
  origin_two.StartAcceptingConnections();

  NavigateToCommittedURLAndWaitForDownload(shell(), referrer_url,
                                           download::DownloadItem::COMPLETE);

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("download"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A file type that Blink can handle should not be downloaded if there are cross
// origin redirects in the middle of the redirect chain.
// TODO(crbug.com/40650833): Fix flakes on various bots and re-enable
// this test.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DISABLED_DownloadAttributeSameOriginRedirectNavigation) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. When
  // the page is loaded, a script simulates a click on the anchor, triggering a
  // download of the target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?target=") + download_url.spec());
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/download. The latter
  // serves an HTML document.
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/download")));
  origin_two.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "text/html", "<title>hello</title>"));

  origin_one.StartAcceptingConnections();
  origin_two.StartAcceptingConnections();

  std::u16string expected_title(u"hello");
  TitleWatcher observer(shell()->web_contents(), expected_title);
  EXPECT_TRUE(
      NavigateToURL(shell(), referrer_url,
                    origin_two.GetURL("/download") /* expected_commit_url */));
  ASSERT_EQ(expected_title, observer.WaitAndGetTitle());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// Tests that if a renderer initiated download triggers cross origin in the
// redirect chain, the visible URL of the current tab shouldn't change.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeSameOriginRedirectNavigationTimeOut) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. When
  // the page is loaded, a script simulates a click on the anchor, triggering a
  // download of the target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?target=") + download_url.spec());
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/download. The latter
  // will time out.
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/download")));

  origin_one.StartAcceptingConnections();

  NavigationStartObserver obs(shell()->web_contents());
  NavigationController::LoadURLParams params(referrer_url);
  params.transition_type = ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR);
  shell()->web_contents()->GetController().LoadURLWithParams(params);
  shell()->web_contents()->Focus();

  // Waiting for 2 navigation to happen, one for the original request, one for
  // the redirect.
  obs.WaitForFinished(2);
  EXPECT_EQ(referrer_url, shell()->web_contents()->GetVisibleURL());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  origin_two.StartAcceptingConnections();
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A download initiated by the user via alt-click on a link should download,
// even when redirected cross origin.
//
// Alt-click doesn't make sense on Android, and download a HTML file results
// in an intent, so just skip.
#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeSameOriginRedirectAltClick) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;
  ASSERT_TRUE(origin_one.InitializeAndListen());
  ASSERT_TRUE(origin_two.InitializeAndListen());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). The suggested filename for the anchor is 'suggested-filename'. We
  // will later send a "real" click to the anchor, triggering a download of the
  // target URL.
  //
  // We construct two test servers; origin_one and origin_two. Once started, the
  // server URLs will differ by the port number. Therefore they will be in
  // different origins.
  GURL download_url = origin_one.GetURL("/ping");
  GURL referrer_url = origin_one.GetURL(
      std::string("/download-attribute.html?noclick=") + download_url.spec());
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/download. The latter
  // serves an HTML document.
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/download")));
  origin_two.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "text/html", "<title>hello</title>"));

  origin_one.StartAcceptingConnections();
  origin_two.StartAcceptingConnections();

  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  EXPECT_TRUE(NavigateToURL(shell(), referrer_url));

  // Alt-click the link.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::Type::kMouseDown, blink::WebInputEvent::kAltKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  shell()
      ->web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);
  mouse_event.SetType(blink::WebInputEvent::Type::kMouseUp);
  shell()
      ->web_contents()
      ->GetPrimaryMainFrame()
      ->GetRenderViewHost()
      ->GetWidget()
      ->ForwardMouseEvent(mouse_event);

  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  base::FilePath file_name = downloads[0]->GetTargetFilePath().BaseName();
#if BUILDFLAG(IS_WIN)
  // Windows file extension depends on system registry.
  EXPECT_TRUE(file_name.value() == FILE_PATH_LITERAL("download.htm") ||
              file_name.value() == FILE_PATH_LITERAL("download.html"));
#else
  EXPECT_EQ(FILE_PATH_LITERAL("download.html"), file_name.value());
#endif

  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}
#endif  // !BUILDFLAG(IS_ANDROID)

// Test that the suggested filename for data: URLs works.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeDataUrl) {
  net::EmbeddedTestServer server;
  ASSERT_TRUE(server.InitializeAndListen());

  GURL url = server.GetURL(std::string(
      "/download-attribute.html?target=data:application/octet-stream, ..."));
  server.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  server.StartAcceptingConnections();

  NavigateToCommittedURLAndWaitForDownload(shell(), url,
                                           download::DownloadItem::COMPLETE);

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("suggested-filename"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  // A link clicked by JavaScript should not have a gesture.
  EXPECT_FALSE(downloads[0]->HasUserGesture());
  ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
}

// A request for a non-existent same-origin resource should result in a
// DownloadItem that's created in an interrupted state.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeServerError) {
  GURL download_url =
      embedded_test_server()->GetURL("/download/does-not-exist");
  GURL document_url = embedded_test_server()->GetURL(
      std::string("/download/download-attribute.html?target=") +
      download_url.spec());

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), document_url);
  WaitForInterrupt(download);

  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());
}

// A cross-origin request that fails before it gets a response from the server
// should result in a network error page.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeNetworkError) {
  SetupErrorInjectionDownloads();
  WebContents* content = shell()->web_contents();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  GURL document_url = embedded_test_server()->GetURL(
      std::string("/download/download-attribute.html?target=") +
      server_url.spec());

  // Simulate a network failure by injecting an error before the response
  // header.
  TestDownloadHttpResponse::Parameters parameters;
  parameters.injected_errors.push(-1);
  parameters.inject_error_cb = inject_error_callback();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  content::TestNavigationManager navigation_document(content, document_url);
  content::TestNavigationManager navigation_download(content, server_url);
  shell()->LoadURL(document_url);
  ASSERT_TRUE(navigation_document.WaitForNavigationFinished());
  ASSERT_TRUE(navigation_download.WaitForNavigationFinished());

  EXPECT_TRUE(navigation_document.was_successful());
  EXPECT_FALSE(navigation_download.was_successful());

  NavigationEntry* navigation_entry =
      shell()->web_contents()->GetController().GetLastCommittedEntry();
  EXPECT_EQ(PAGE_TYPE_ERROR, navigation_entry->GetPageType());
  EXPECT_EQ(server_url, navigation_entry->GetURL());
}

// A request that fails due to it being rejected by policy should result in a
// corresponding navigation.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeInvalidURL) {
  GURL url = embedded_test_server()->GetURL(
      "/download/download-attribute.html?target=about:version");
  auto observer =
      std::make_unique<content::TestNavigationObserver>(GURL(kBlockedURL));
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();
  shell()->LoadURL(url);
  observer->WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeBlobURL) {
  GURL document_url =
      embedded_test_server()->GetURL("/download/download-attribute-blob.html");
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), document_url);
  WaitForCompletion(download);

  EXPECT_STREQ(FILE_PATH_LITERAL("suggested-filename.txt"),
               download->GetTargetFilePath().BaseName().value().c_str());
}

class DownloadContentSameSiteCookieTest
    : public DownloadContentTest,
      public ::testing::WithParamInterface<bool> {
 public:
  DownloadContentSameSiteCookieTest() {
    inner_feature_list_.InitWithFeatureState(
        net::features::kCookieSameSiteConsidersRedirectChain,
        DoesCookieSameSiteConsiderRedirectChain());
  }

  bool DoesCookieSameSiteConsiderRedirectChain() { return GetParam(); }

 private:
  base::test::ScopedFeatureList inner_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DownloadContentSameSiteCookieTest,
                       DownloadAttributeSameSiteCookie) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  net::EmbeddedTestServer test_server;
  ASSERT_TRUE(test_server.InitializeAndListen());

  test_server.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  test_server.RegisterRequestHandler(
      CreateEchoCookieHandler("/downloadcookies"));

  GURL echo_cookie_url = test_server.GetURL(kOriginOne, "/downloadcookies");
  test_server.RegisterRequestHandler(
      CreateRedirectHandler("/server-redirect", echo_cookie_url));

  test_server.StartAcceptingConnections();

  // download-attribute-same-site-cookie sets two cookies. One "A=B" is set with
  // SameSite=Strict. The other one "B=C" doesn't have this flag. In general
  // a[download] should behave the same as a top level navigation.
  //
  // The page then simulates a click on an <a download> link whose target is the
  // /echoheader handler on the same origin.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(),
      test_server.GetURL(
          kOriginOne,
          std::string("/download-attribute-same-site-cookie.html?target=") +
              echo_cookie_url.spec()));
  WaitForCompletion(download);

  std::string file_contents;
  ASSERT_TRUE(
      base::ReadFileToString(download->GetTargetFilePath(), &file_contents));

  // Initiator and target are same-origin. Both cookies should have been
  // included in the request.
  EXPECT_STREQ("A=B; B=C", file_contents.c_str());

  // The test isn't complete without verifying that the initiator isn't being
  // incorrectly set to be the same as the resource origin. The
  // download-attribute test page doesn't set any cookies but creates a download
  // via a <a download> link to the target URL. In this case:
  //
  //  Initiator origin: kOriginTwo
  //  Resource origin: kOriginOne
  //  First-party origin: kOriginOne
  download = StartDownloadAndReturnItem(
      shell(), test_server.GetURL(
                   kOriginTwo, std::string("/download-attribute.html?target=") +
                                   echo_cookie_url.spec()));
  WaitForCompletion(download);

  ASSERT_TRUE(
      base::ReadFileToString(download->GetTargetFilePath(), &file_contents));

  // The initiator and the target are not same-origin. Only the second cookie
  // should be sent along with the request.
  EXPECT_STREQ("B=C", file_contents.c_str());

  // OriginOne redirects through OriginTwo. Because the redirect chain contains
  // a cross-site redirect, SameSite=Strict cookies are not sent (if redirect
  // chains are considered).
  //
  //  Initiator origin: kOriginOne
  //  Redirect chain contains: kOriginTwo
  //  Resource origin: kOriginOne
  //  First-party origin: kOriginOne
  GURL redirect_url = test_server.GetURL(kOriginTwo, "/server-redirect");
  download = StartDownloadAndReturnItem(
      shell(), test_server.GetURL(
                   kOriginOne, std::string("/download-attribute.html?target=") +
                                   redirect_url.spec()));
  WaitForCompletion(download);

  ASSERT_TRUE(
      base::ReadFileToString(download->GetTargetFilePath(), &file_contents));
  if (DoesCookieSameSiteConsiderRedirectChain()) {
    EXPECT_STREQ("B=C", file_contents.c_str());
  } else {
    EXPECT_STREQ("A=B; B=C", file_contents.c_str());
  }
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         DownloadContentSameSiteCookieTest,
                         ::testing::Bool());

// The file empty.bin is served with a MIME type of application/octet-stream.
// The content body is empty. Make sure this case is handled properly and we
// don't regress on http://crbug.com/320394.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadGZipWithNoContent) {
  NavigateToURLAndWaitForDownload(
      shell(), embedded_test_server()->GetURL("/download/empty.bin"),
      download::DownloadItem::COMPLETE);
  // That's it. This should work without crashing.
}

// Make sure that sniffed MIME types are correctly passed through to the
// download item.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, SniffedMimeType) {
  download::DownloadItem* item = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL("/download/gzip-content.gz"));
  WaitForCompletion(item);

  EXPECT_STREQ("application/x-gzip", item->GetMimeType().c_str());
  EXPECT_TRUE(item->GetOriginalMimeType().empty());
}

// Verify that for download that is not triggered by navigation, MIME sniffing
// is working.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, SniffedMimeTypeForDownloadURL) {
  GURL download_url =
      embedded_test_server()->GetURL("/download/gzip-content.gz");
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      download_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  // Download URL without navigation.
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Verify download failed.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());
  EXPECT_STREQ("application/x-gzip", downloads[0]->GetMimeType().c_str());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DuplicateContentDisposition) {
  // double-content-disposition.txt is served with two Content-Disposition
  // headers, both of which are identical.
  NavigateToURLAndWaitForDownload(
      shell(),
      embedded_test_server()->GetURL(
          "/download/double-content-disposition.txt"),
      download::DownloadItem::COMPLETE);

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("Jumboshrimp.txt"),
            downloads[0]->GetTargetFilePath().BaseName().value());
}

// Test that the network isolation key is populated for:
// (1) <a download> triggered download request that doesn't go through the
// navigation path
// (2) the request resuming an interrupted download.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       AnchorDownload_Resume_IsolationInfoPopulated) {
  SetupEnsureNoPendingDownloads();

  GURL slow_download_url = embedded_test_server()->GetURL(
      kOriginTwo, SlowDownloadHttpResponse::kKnownSizeUrl);
  url::Origin download_origin = url::Origin::Create(slow_download_url);
  net::IsolationInfo expected_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kMainFrame, download_origin,
      download_origin, net::SiteForCookies::FromOrigin(download_origin));

  GURL frame_url = embedded_test_server()->GetURL(
      kOriginTwo,
      "/download/download-attribute.html?noclick=" + slow_download_url.spec());
  GURL document_url = embedded_test_server()->GetURL(
      kOriginOne, "/download/iframe-host.html?target=" + frame_url.spec());

  // Load a page that contains a cross-origin iframe, where the iframe contains
  // a <a download> link same-origin to the iframe's origin.
  TestNavigationObserver same_tab_observer(shell()->web_contents(), 1);
  shell()->LoadURL(document_url);
  same_tab_observer.Wait();

  // Click the <a download> link in the child frame.
  download::DownloadItem* download_item = nullptr;
  ExpectRequestIsolationInfo(
      slow_download_url, expected_isolation_info,
      base::BindLambdaForTesting([&]() {
        DownloadInProgressObserver observer(DownloadManagerForShell(shell()));
        EXPECT_TRUE(
            ExecJs(ChildFrameAt(shell()->web_contents(), 0),
                   "var anchorElement = document.querySelector('a[download]'); "
                   "anchorElement.click();"));
        download_item = observer.WaitAndGetInProgressDownload();
      }));

  download_item->SimulateErrorForTesting(
      download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED);
  EXPECT_EQ(download::DownloadItem::INTERRUPTED, download_item->GetState());

  ExpectRequestIsolationInfo(
      slow_download_url, expected_isolation_info,
      base::BindLambdaForTesting([&]() { download_item->Resume(true); }));

  EXPECT_EQ(download::DownloadItem::IN_PROGRESS, download_item->GetState());
  download_item->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeSameOriginIFrame) {
  GURL frame_url = embedded_test_server()->GetURL(
      "/download/download-attribute.html?target=/download/download-test.lib");
  GURL document_url = embedded_test_server()->GetURL(
      "/download/iframe-host.html?target=" + frame_url.spec());
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), document_url);
  WaitForCompletion(download);

  EXPECT_STREQ(FILE_PATH_LITERAL("suggested-filename"),
               download->GetTargetFilePath().BaseName().value().c_str());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeCrossOriginIFrame) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;

  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  origin_two.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  ASSERT_TRUE(origin_one.Start());
  ASSERT_TRUE(origin_two.Start());

  GURL download_url = origin_two.GetURL("/download-test.lib");
  url::Origin download_origin = url::Origin::Create(download_url);
  // The IsolationInfo of the download should be the same as that of a top-level
  // navigation to the download.
  net::IsolationInfo expected_isolation_info = net::IsolationInfo::Create(
      net::IsolationInfo::RequestType::kSubFrame, download_origin,
      download_origin, net::SiteForCookies::FromOrigin(download_origin));

  GURL frame_url = origin_one.GetURL("/download-attribute.html?target=" +
                                     download_url.spec());
  GURL::Replacements replacements;
  replacements.SetHostStr("localhost");
  frame_url = frame_url.ReplaceComponents(replacements);
  GURL document_url =
      origin_two.GetURL("/iframe-host.html?target=" + frame_url.spec());
  download::DownloadItem* download = nullptr;
  ExpectRequestIsolationInfo(
      download_url, expected_isolation_info, base::BindLambdaForTesting([&]() {
        download = StartDownloadAndReturnItem(shell(), document_url);
      }));

  WaitForCompletion(download);

  EXPECT_STREQ(FILE_PATH_LITERAL("download-test.lib"),
               download->GetTargetFilePath().BaseName().value().c_str());
}

// Verify parallel download in normal case.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, ParallelDownloadComplete) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = 5097152;

  RunCompletionTest(parameters);
}

// When the last request is rejected by the server, other parallel requests
// should take over and complete the download.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, LastRequestRejected) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = 5097152;
  // The 3rd request will always fail. Other requests should take over.
  parameters.SetResponseForRangeRequest(3398000, -1, k404Response);

  RunCompletionTest(parameters);
}

// When the second request is rejected by the server, other parallel requests
// should take over and complete the download.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, SecondRequestRejected) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = 5097152;
  // The 2nd request will always fail. Other requests should take over.
  parameters.SetResponseForRangeRequest(1699000, 2000000, k404Response);
  RunCompletionTest(parameters);
}

// The server will only accept the original request, and reject all other
// requests. The original request should complete the whole download.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, OnlyFirstRequestValid) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = 5097152;

  // 2nd and 3rd request will fail, the original request should complete the
  // download.
  parameters.SetResponseForRangeRequest(1000, -1, k404Response);
  RunCompletionTest(parameters);
}

// The server will send Accept-Ranges header without partial response.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, NoPartialResponse) {
  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = 5097152;
  parameters.support_byte_ranges = true;
  parameters.support_partial_response = false;

  RunCompletionTest(parameters);
}

// Verify parallel download resumption.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, Resumption) {
  // Create the received slices data, the last request is not finished and the
  // server will send more data to finish the last slice.
  std::vector<download::DownloadItem::ReceivedSlice> received_slices = {
      download::DownloadItem::ReceivedSlice(0, 1000),
      download::DownloadItem::ReceivedSlice(1000000, 1000),
      download::DownloadItem::ReceivedSlice(2000000, 1000,
                                            false /* finished */)};

  RunResumptionTest(received_slices, 3000000, kTestRequestCount,
                    true /* support_partial_response */);
}

// Verifies that if the last slice is finished, parallel download resumption
// can complete.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, ResumptionLastSliceFinished) {
  // Create the received slices data, last slice is actually finished.
  std::vector<download::DownloadItem::ReceivedSlice> received_slices = {
      download::DownloadItem::ReceivedSlice(0, 1000),
      download::DownloadItem::ReceivedSlice(1000000, 1000),
      download::DownloadItem::ReceivedSlice(2000000, 1000000,
                                            true /* finished */)};

  // The server shouldn't receive an additional request, since the last slice
  // is marked as finished.
  RunResumptionTest(received_slices, 3000000, kTestRequestCount - 1,
                    true /* support_partial_response */);
}

// Verify parallel download resumption when only 1 slice was created in previous
// attempt.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, ResumptionWithOnlyOneSlice) {
  // Create the received slices data with only 1 slice.
  std::vector<download::DownloadItem::ReceivedSlice> received_slices = {
      download::DownloadItem::ReceivedSlice(0, 1000, false /* finished */)};

  // Only 1 request should be sent.
  RunResumptionTest(received_slices, 3000000, 1,
                    true /* support_partial_response */);
}

// Verifies that if the last slice is finished, but the database record is not
// finished, which may happen in database migration.
// When the server sends HTTP range not satisfied, the download can complete.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, ResumptionLastSliceUnfinished) {
  // Create the received slices data, last slice is actually finished.
  std::vector<download::DownloadItem::ReceivedSlice> received_slices = {
      download::DownloadItem::ReceivedSlice(0, 1000),
      download::DownloadItem::ReceivedSlice(1000000, 1000),
      download::DownloadItem::ReceivedSlice(2000000, 1000000,
                                            false /* finished */)};

  // Client will send an out of range request where server will send back HTTP
  // range not satisfied, and download can complete.
  RunResumptionTest(received_slices, 3000000, kTestRequestCount,
                    true /* support_partial_response */);
}

// Verify that if server doesn't support partial response, resuming a parallel
// download should complete the download.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, ResumptionNoPartialResponse) {
  // Create the received slices data, the last request is not finished and the
  // server will send more data to finish the last slice.
  std::vector<download::DownloadItem::ReceivedSlice> received_slices = {
      download::DownloadItem::ReceivedSlice(0, 1000),
      download::DownloadItem::ReceivedSlice(1000000, 1000),
      download::DownloadItem::ReceivedSlice(2000000, 1000,
                                            false /* finished */)};

  RunResumptionTest(received_slices, 3000000, kTestRequestCount,
                    false /* support_partial_response */);
}

// Verify that if a temporary error happens to one of the parallel request,
// resuming a parallel download should still complete.
// Flaky on fuchsia: https://crbug.com/1492656
#if BUILDFLAG(IS_FUCHSIA)
#define MAYBE_ResumptionMiddleSliceTemporaryError \
  DISABLED_ResumptionMiddleSliceTemporaryError
#else
#define MAYBE_ResumptionMiddleSliceTemporaryError \
  ResumptionMiddleSliceTemporaryError
#endif
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest,
                       MAYBE_ResumptionMiddleSliceTemporaryError) {
  // Create the received slices data.
  std::vector<download::DownloadItem::ReceivedSlice> received_slices = {
      download::DownloadItem::ReceivedSlice(0, 1000),
      download::DownloadItem::ReceivedSlice(1000000, 1000),
      download::DownloadItem::ReceivedSlice(2000000, 1000,
                                            false /* finished */)};

  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = 3000000;
  parameters.connection_type = net::HttpConnectionInfo::kHTTP1_1;
  // The 2nd slice will fail. Once the first and the third slices
  // complete, download will resume on the 2nd slice.
  parameters.SetResponseForRangeRequest(1000000, 1010000, k404Response,
                                        true /* is_transient */);

  // A total of 4 requests will be sent, 3 during the initial attempt, and 1
  // for the retry attempt on the 2nd slice.
  RunResumptionTestWithParameters(received_slices, kTestRequestCount + 1,
                                  parameters);
}

// Verify that if the second request fails after the beginning request takes
// over and completes its slice, download should complete.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, MiddleSliceDelayedError) {
  const int64_t kFileSize = 5097152;

  scoped_refptr<TestFileErrorInjector> injector(
      TestFileErrorInjector::Create(DownloadManagerForShell(shell())));

  TestFileErrorInjector::FileErrorInfo err = {
      TestFileErrorInjector::FILE_OPERATION_WRITE, 1,
      download::DOWNLOAD_INTERRUPT_REASON_FILE_NO_SPACE};
  err.data_write_offset = 1699050;
  injector->InjectError(err);
  TestDownloadHttpResponse::Parameters parameters;
  parameters.etag = "ABC";
  parameters.size = kFileSize;
  parameters.connection_type = net::HttpConnectionInfo::kHTTP1_1;
  // The 2nd response will be dalyed.
  parameters.SetResponseForRangeRequest(1699000, 2000000, k404Response,
                                        true /* is_transient */,
                                        true /* delay_response */);

  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestRequestPauseHandler request_pause_handler;
  parameters.on_pause_handler = request_pause_handler.GetOnPauseHandler();
  // Send some data for the first request and pause it so download won't
  // complete before other parallel requests are created.
  parameters.pause_offset = kPauseOffset;
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), server_url);

  // Wait for the 3rd request to complete first.
  test_response_handler()->WaitUntilCompletion(1);
  ReceivedSlicesCountingObserver slices_counting_observer;
  slices_counting_observer.WaitForFinished(download, 2);
  std::vector<download::DownloadItem::ReceivedSlice> received_slices =
      download->GetReceivedSlices();
  EXPECT_EQ(received_slices[1].offset + received_slices[1].received_bytes,
            kFileSize);

  // Now resume the first request and wait for it to complete, including writing
  // the whole file.
  request_pause_handler.Resume();
  ReceivedBytesCountingObserver bytes_counting_observer;
  bytes_counting_observer.WaitForFinished(download, kFileSize);
  // Note that download is not yet completed even though the whole file is
  // downloaded - second request is not yet processed.
  EXPECT_EQ(download->GetState(),
            download::DownloadItem::DownloadState::IN_PROGRESS);

  // Dispatch the delayed response, and wait for download to complete.
  test_response_handler()->DispatchDelayedResponses();
  WaitForCompletion(download);
  test_response_handler()->WaitUntilCompletion(3u);
  const TestDownloadResponseHandler::CompletedRequests& completed_requests =
      test_response_handler()->completed_requests();
  EXPECT_EQ(3u, completed_requests.size());
  WaitForCompletion(download);
  ReadAndVerifyFileContents(parameters.pattern_generator_seed, parameters.size,
                            download->GetTargetFilePath());
}

// Test to verify that the browser-side enforcement of X-Frame-Options does
// not impact downloads. Since XFO is only checked for subframes, this test
// initiates a download in an iframe and expects it to succeed.
// See https://crbug.com/717971.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadIgnoresXFO) {
  GURL main_url(embedded_test_server()->GetURL(
      "a.test", "/cross_site_iframe_factory.html?a.test(b.test)"));
  GURL download_url(
      embedded_test_server()->GetURL("/download/download-with-xfo-deny.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  NavigateFrameToURL(web_contents->GetPrimaryFrameTree().root()->child_at(0),
                     download_url);
  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("foo"),
            downloads[0]->GetTargetFilePath().BaseName().value());
}

// Verify that the response body of non-successful server response can be
// downloaded to a file, when |fetch_error_body| sets to true.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, FetchErrorResponseBody) {
  net::EmbeddedTestServer server;
  const std::string kNotFoundURL = "/404notfound";
  const std::string kNotFoundResponseBody = "This is response body.";

  server.RegisterRequestHandler(CreateBasicResponseHandler(
      kNotFoundURL, net::HTTP_NOT_FOUND, base::StringPairs(), "text/html",
      kNotFoundResponseBody));
  ASSERT_TRUE(server.Start());
  GURL url = server.GetURL(kNotFoundURL);

  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), url, TRAFFIC_ANNOTATION_FOR_TESTS));
  // Fetch non-successful response body.
  download_parameters->set_fetch_error_body(true);

  DownloadManager* download_manager = DownloadManagerForShell(shell());
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  download_manager->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(1u, items.size());

  // Verify the error response body in the file.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string file_content;
    ASSERT_TRUE(
        base::ReadFileToString(items[0]->GetTargetFilePath(), &file_content));
    EXPECT_EQ(kNotFoundResponseBody, file_content);
  }
}

// Verify that the upload body of a request is received correctly by the server.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, UploadBytes) {
  net::EmbeddedTestServer server;
  const std::string kUploadURL = "/upload";
  std::string kUploadString = "Test upload body";

  server.RegisterRequestHandler(base::BindRepeating(&HandleUploadRequest));
  ASSERT_TRUE(server.Start());
  GURL url = server.GetURL(kUploadURL);

  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), url, TRAFFIC_ANNOTATION_FOR_TESTS));

  download_parameters->set_post_body(
      network::ResourceRequestBody::CreateFromBytes(kUploadString.data(),
                                                    kUploadString.size()));

  DownloadManager* download_manager = DownloadManagerForShell(shell());
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  download_manager->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(1u, items.size());

  // Verify the response body in the file. It should match the request content.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string file_content;
    ASSERT_TRUE(
        base::ReadFileToString(items[0]->GetTargetFilePath(), &file_content));
    EXPECT_EQ(kUploadString, file_content);
  }
}

// Verify the case that the first response is HTTP 200, and then interrupted,
// and the second response is HTTP 404, the response body of 404 should be
// fetched.
// Also verify the request header is correctly piped to download item.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, FetchErrorResponseBodyResumption) {
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  // Wait for an interrupted download.
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), server_url, TRAFFIC_ANNOTATION_FOR_TESTS));
  download_parameters->set_fetch_error_body(true);
  download_parameters->add_request_header("header_key", "header_value");

  DownloadManager* download_manager = DownloadManagerForShell(shell());

  auto observer = std::make_unique<content::DownloadTestObserverInterrupted>(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);
  download_manager->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(1u, items.size());

  // Now server will start to response 404 with empty body.
  TestDownloadHttpResponse::StartServingStaticResponse(k404Response,
                                                       server_url);
  download::DownloadItem* download = items[0];

  // The fetch error body should be cached in download item. The download should
  // start from beginning.
  download->Resume(false);
  WaitForCompletion(download);

  // The file should be empty.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string file_content;
    ASSERT_TRUE(
        base::ReadFileToString(items[0]->GetTargetFilePath(), &file_content));
    EXPECT_EQ(std::string(), file_content);
  }

  // Additional request header should be sent.
  test_response_handler()->WaitUntilCompletion(2u);
  const auto& request = test_response_handler()->completed_requests().back();
  auto it = request->http_request.headers.find("header_key");
  EXPECT_TRUE(it != request->http_request.headers.end());
  EXPECT_EQ(request->http_request.headers["header_key"],
            std::string("header_value"));
}

// Verify WebUI download will success with an associated renderer process.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadFromWebUI) {
  GURL webui_url(GetWebUIURL("resources/images/error.svg"));
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));
  SetupEnsureNoPendingDownloads();

  // Creates download parameters with renderer process information.
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), webui_url, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());
}

// Verify WebUI download will gracefully fail without an associated renderer
// process.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadFromWebUIWithoutRenderer) {
  GURL webui_url("chrome://resources/images/error.svg");
  EXPECT_TRUE(NavigateToURL(shell(), webui_url));
  SetupEnsureNoPendingDownloads();

  // Creates download parameters without any renderer process information.
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      webui_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  // WebUI or other UrlLoaderFacotry will not handle request without a valid
  // RenderFrameHost, download should gracefully fail without triggering
  // crash.
  ASSERT_EQ(download::DownloadItem::INTERRUPTED, downloads[0]->GetState());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, SaveImageAt) {
  // Navigate to a page containing a data-URL image in the top-left corner.
  GURL main_url(
      embedded_test_server()->GetURL("/download/page_with_data_image.html"));
  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  // Ask the frame to save a data-URL image at the given coordinates.
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  shell()->web_contents()->GetPrimaryMainFrame()->SaveImageAt(100, 100);
  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  // Verify that there was one, appropriately named download.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  EXPECT_EQ(FILE_PATH_LITERAL("download.png"),
            downloads[0]->GetTargetFilePath().BaseName().value());

  // Verify file contents.
  std::string expected_content;
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(ReadFileToString(GetTestFilePath("media", "blackwhite.png"),
                                 &expected_content));
  }
  ASSERT_TRUE(VerifyFile(downloads[0]->GetFullPath(), expected_content,
                         expected_content.size()));
}

// Test fixture for forcing MHTML download.
class MhtmlDownloadTest : public DownloadContentTest {
 protected:
  void SetUpOnMainThread() override {
    DownloadContentTest::SetUpOnMainThread();

    browser_client_ = std::make_unique<DownloadTestContentBrowserClient>();
    // Force downloading the MHTML.
    browser_client_->set_allowed_rendering_mhtml_over_http(false);
    // Enable RegisterNonNetworkNavigationURLLoaderFactories for
    // test white list for non http shemes which should not trigger
    // download.
    browser_client_->enable_register_non_network_url_loader(true);
  }

  void TearDownOnMainThread() override {
    browser_client_.reset();
    DownloadContentTest::TearDownOnMainThread();
  }

 private:
  std::unique_ptr<DownloadTestContentBrowserClient> browser_client_;
};

// Test allow list for non http schemes which should not trigger
// download for mhtml.
IN_PROC_BROWSER_TEST_F(MhtmlDownloadTest,
                       AllowListForNonHTTPNotTriggerDownload) {
#if BUILDFLAG(IS_ANDROID)
  // "content://" is an protocol on Android.
  GURL content_url("content://non_download.mhtml");
  NavigateToCommittedURLAndExpectNoDownload(shell(), content_url);
#endif
  GURL file_url("file:///non_download.mhtml");
  NavigateToCommittedURLAndExpectNoDownload(shell(), file_url);
}

#if defined(THREAD_SANITIZER)
// Flaky on TSAN https://crbug.com/932092
#define MAYBE_ForceDownloadMultipartRelatedPage \
  DISABLED_ForceDownloadMultipartRelatedPage
#else
#define MAYBE_ForceDownloadMultipartRelatedPage \
  ForceDownloadMultipartRelatedPage
#endif
IN_PROC_BROWSER_TEST_F(MhtmlDownloadTest,
                       MAYBE_ForceDownloadMultipartRelatedPage) {
  NavigateToURLAndWaitForDownload(
      shell(),
      // .mhtml file is mapped to "multipart/related" by the test server.
      embedded_test_server()->GetURL("/download/hello.mhtml"),
      download::DownloadItem::COMPLETE);
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || defined(ADDRESS_SANITIZER)
// Flaky https://crbug.com/852073
#define MAYBE_ForceDownloadMessageRfc822Page \
  DISABLED_ForceDownloadMessageRfc822Page
#else
#define MAYBE_ForceDownloadMessageRfc822Page ForceDownloadMessageRfc822Page
#endif
IN_PROC_BROWSER_TEST_F(MhtmlDownloadTest,
                       MAYBE_ForceDownloadMessageRfc822Page) {
  NavigateToURLAndWaitForDownload(
      shell(),
      // .mht file is mapped to "message/rfc822" by the test server.
      embedded_test_server()->GetURL("/download/test.mht"),
      download::DownloadItem::COMPLETE);
}

// Test fixture for loading MHTML.
class MhtmlLoadingTest : public DownloadContentTest {
 protected:
  // Return an URL for loading a local test file.
  GURL GetFileURL(const base::FilePath::CharType* file_path) {
    base::FilePath path;
    CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &path));
    path = path.Append(GetTestDataFilePath());
    path = path.Append(file_path);
    return GURL("file:" + path.AsUTF8Unsafe());
  }
};

IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest,
                       AllowRenderMultipartRelatedPageFromFile) {
  GURL url = GetFileURL(FILE_PATH_LITERAL("download/hello.mhtml"));
  auto observer = std::make_unique<content::TestNavigationObserver>(url);
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();

  EXPECT_TRUE(NavigateToURL(shell(), url));

  observer->WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest, AllowRenderMessageRfc822PageFromFile) {
  GURL url = GetFileURL(FILE_PATH_LITERAL("download/test.mht"));
  auto observer = std::make_unique<content::TestNavigationObserver>(url);
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();

  EXPECT_TRUE(NavigateToURL(shell(), url));

  observer->WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest,
                       DisallowRenderMultipartRelatedPageFromHTTP) {
  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response(&server, "/");
  EXPECT_TRUE(server.Start());
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));

  GURL url = server.GetURL(kOrigin, "/");

  shell()->LoadURL(url);

  response.WaitForRequest();
  response.Send(net::HTTP_OK, "multipart/related");
  response.Done();

  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));
}

IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest,
                       DisallowRenderMessageRfc822PageFromHTTP) {
  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse response(&server, "/");
  EXPECT_TRUE(server.Start());
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));

  GURL url = server.GetURL(kOrigin, "/");

  shell()->LoadURL(url);

  response.WaitForRequest();
  response.Send(net::HTTP_OK, "message/rfc822");
  response.Done();

  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));
}

// Regression test for https://crbug.com/1171765
IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest, DisallowRenderMessageRfc822Iframe) {
  net::EmbeddedTestServer server;
  net::test_server::ControllableHttpResponse main_response(&server, "/main");
  net::test_server::ControllableHttpResponse sub_response(&server, "/sub");
  EXPECT_TRUE(server.Start());

  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));

  GURL main_url = server.GetURL(kOrigin, "/main");
  GURL sub_url = server.GetURL(kOrigin, "/sub");

  shell()->LoadURL(main_url);

  main_response.WaitForRequest();
  main_response.Send(net::HTTP_OK, "text/html",
                     "<iframe src='./sub'></iframe>");
  main_response.Done();

  sub_response.WaitForRequest();
  sub_response.Send(net::HTTP_OK, "message/rfc822");
  sub_response.Done();

  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));
}

// Verify that downloads not triggered by navigation are discarded when
// initiated from a non-active page.
// Navigation downloads won't reach the DownloadManager. That is tested in
// PrerenderBrowserTest.{DownloadInMainFrame,DownloadInSubframe}.
IN_PROC_BROWSER_TEST_F(DownloadPrerenderTest, DiscardNonNavigationDownload) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kPrerenderingUrl =
      embedded_test_server()->GetURL("/empty.html?prerendering");
  const GURL kDownloadUrl =
      embedded_test_server()->GetURL("/download/download-test.lib");

  EXPECT_TRUE(NavigateToURL(shell(), kInitialUrl));

  // Create a prerendered page.
  FrameTreeNodeId host_id = prerender_helper()->AddPrerender(kPrerenderingUrl);
  auto* render_frame_host =
      prerender_helper()->GetPrerenderedMainFrameHost(host_id);
  auto* web_contents = shell()->web_contents();
  test::PrerenderHostObserver host_observer(*web_contents, host_id);

  // Do a download without navigation from the prerendered RenderFrameHost. The
  // download should not reach the download manager.
  auto* download_manager = DownloadManagerForShell(shell());
  MockDownloadManagerObserver dm_observer(download_manager);
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);
  EXPECT_CALL(dm_observer, OnDownloadDropped(_)).Times(0);

  auto params = blink::mojom::DownloadURLParams::New();
  params->url = kDownloadUrl;
  static_cast<RenderFrameHostImpl*>(render_frame_host)
      ->DownloadURL(std::move(params));

  // Do a download without navigation, from the download manager. In this
  // case, the download will be dropped.
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);
  EXPECT_CALL(dm_observer, OnDownloadDropped(_)).Times(1);

  // Create download parameters with the renderer process information from the
  // prerendered page and mark it as rendered-initiated, otherwise the download
  // won't be checked.
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      kDownloadUrl, render_frame_host->GetProcess()->GetID(),
      render_frame_host->GetRoutingID(), TRAFFIC_ANNOTATION_FOR_TESTS);
  download_parameters->set_content_initiated(true);
  download_manager->DownloadUrl(std::move(download_parameters));

  // No navigations were done, so the prerendered page wasn't activated.
  EXPECT_FALSE(host_observer.was_activated());

  // Verify there were no downloads.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  EXPECT_TRUE(downloads.empty());
}

// Verify that downloads not triggered by navigation are discarded when
// initiated from a fenced frame.
IN_PROC_BROWSER_TEST_F(DownloadFencedFrameTest, DiscardNonNavigationDownload) {
  const GURL kInitialUrl = embedded_test_server()->GetURL("/empty.html");
  const GURL kFencedFrameUrl =
      embedded_test_server()->GetURL("/fenced_frames/title1.html");
  const GURL kDownloadUrl =
      embedded_test_server()->GetURL("/download/download-test.lib");

  // Create fenced frame
  EXPECT_TRUE(NavigateToURL(shell(), kInitialUrl));
  RenderFrameHost* fenced_frame_host = CreateFencedFrame(
      shell()->web_contents()->GetPrimaryMainFrame(), kFencedFrameUrl);

  // Do a download without navigation from the fenced frame RenderFrameHost.
  // The download will be dropped.
  auto* download_manager =
      fenced_frame_host->GetBrowserContext()->GetDownloadManager();
  MockDownloadManagerObserver dm_observer(download_manager);
  EXPECT_CALL(dm_observer, OnDownloadCreated(_, _)).Times(0);
  EXPECT_CALL(dm_observer, OnDownloadDropped(_)).Times(1);

  auto params = blink::mojom::DownloadURLParams::New();
  params->url = kDownloadUrl;
  static_cast<RenderFrameHostImpl*>(fenced_frame_host)
      ->DownloadURL(std::move(params));

  // Verify there were no downloads.
  EXPECT_TRUE(EnsureNoPendingDownloads());
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  download_manager->GetAllDownloads(&downloads);
  EXPECT_TRUE(downloads.empty());
}

// A download triggered by clicking on a link with a |download| attribute should
// have the user-gesture flag set.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributePreservesUserGesture) {
  net::EmbeddedTestServer server;
  ASSERT_TRUE(server.InitializeAndListen());

  // The download-attribute.html page contains an anchor element whose href is
  // set to the value of the query parameter (specified as |target| in the URL
  // below). When the page is loaded, a script simulates a click on the anchor,
  // triggering a download of the target URL.
  GURL download_url = server.GetURL("/download");
  GURL referrer_url =
      server.GetURL(std::string("/download-attribute.html?noclick&target=") +
                    download_url.spec());
  server.ServeFilesFromDirectory(GetTestFilePath("download", ""));

  // download-attribute.html initiates a download of /download.
  server.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "application/octet-stream", "Hello"));

  server.StartAcceptingConnections();
  std::unique_ptr<DownloadTestObserver> observer(
      CreateInProgressWaiter(shell(), 1));

  // Load the download page and click on the link.
  EXPECT_TRUE(NavigateToURL(shell(), referrer_url));
  content::SimulateMouseClickOrTapElementWithId(shell()->web_contents(),
                                                "downloadlink");

  // Wait for the download.
  observer->WaitForFinished();

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  // Ensure that the download is treated as having a user-gesture.
  EXPECT_EQ(FILE_PATH_LITERAL("suggested-filename"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  EXPECT_TRUE(downloads[0]->HasUserGesture());

  ASSERT_TRUE(server.ShutdownAndWaitUntilComplete());
}

using DownloadRangeTestParams =
    std::tuple<int64_t /*starting byte in range request*/,
               int64_t /*ending byte in range request*/,
               int64_t /*starting byte in download file*/,
               int64_t /*expected length*/>;

// Browser test for arbitrary range download. This is for download system
// caller to explicitly ask for range request, not for parallel download and
// resumption that internally use range requests.
class DownloadRangeTest
    : public DownloadContentTest,
      public ::testing::WithParamInterface<DownloadRangeTestParams> {
 public:
  DownloadRangeTest() = default;
  ~DownloadRangeTest() override = default;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    DownloadRangeTest,
    testing::Values(/*bytes=10-19, fetch 10 bytes*/
                    std::make_tuple(10, 19, 10, 10),
                    /*bytes=10-, fetch starting from 10th byte to the end*/
                    std::make_tuple(10, download::kInvalidRange, 10, 136),
                    /*bytes=-5*, fetch the last 5 bytes*/
                    std::make_tuple(download::kInvalidRange, 5, 141, 5)));

// Test to download with range request with
// |DownloadUrlParameters::set_range_request_offset|.
IN_PROC_BROWSER_TEST_P(DownloadRangeTest, ArbitraryDownloadRangeTest) {
  GURL download_url =
      embedded_test_server()->GetURL("/download/download-test.lib");
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      download_url, TRAFFIC_ANNOTATION_FOR_TESTS);
  // Perform a range download.
  download_parameters->set_use_if_range(false);
  download_parameters->set_range_request_offset(std::get<0>(GetParam()),
                                                std::get<1>(GetParam()));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Verify download completed.
  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  EXPECT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());

  // Verify the partial file is downloaded correctly.
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string whole_file, partial_file;
    ASSERT_TRUE(base::ReadFileToString(
        GetTestFilePath("download", "download-test.lib"), &whole_file));
    ASSERT_TRUE(base::ReadFileToString(downloads[0]->GetTargetFilePath(),
                                       &partial_file));
    EXPECT_EQ(
        whole_file.substr(std::get<2>(GetParam()), std::get<3>(GetParam())),
        partial_file);
  }
}

class DownloadRangeResumptionTest : public DownloadContentTest {
 public:
  DownloadRangeResumptionTest() = default;
  ~DownloadRangeResumptionTest() override = default;
};

// Test to download resumption from a partially downloaded file with range
// request with |DownloadUrlParameters::set_range_request_offset|.
IN_PROC_BROWSER_TEST_F(DownloadRangeResumptionTest,
                       ArbitraryDownloadRangeResumptionTest) {
  // Make range download interrupted at certain position.
  SetupErrorInjectionDownloads();
  GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
  GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
  TestDownloadHttpResponse::Parameters parameters =
      TestDownloadHttpResponse::Parameters::WithSingleInterruption(
          inject_error_callback());
  EXPECT_EQ(51200, parameters.injected_errors.front());

  // Make sure when auto resume from failure point, the server can response
  // correctly.
  parameters.SetResponseForRangeRequest(
      51200, 100000,
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes 51200-100000/48801\r\n"
      "\r\n");
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  // Perform a range download.
  auto download_parameters = std::make_unique<download::DownloadUrlParameters>(
      server_url, TRAFFIC_ANNOTATION_FOR_TESTS);

  download_parameters->set_use_if_range(false);
  download_parameters->set_range_request_offset(10, 100000);

  DownloadManager* download_manager = DownloadManagerForShell(shell());
  std::unique_ptr<DownloadTestObserverInterrupted> observer =
      std::make_unique<DownloadTestObserverInterrupted>(
          download_manager, 1,
          DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL);

  download_manager->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Now clear the error and resume it.
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  std::vector<raw_ptr<download::DownloadItem, VectorExperimental>> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  EXPECT_EQ(1u, downloads.size());
  download::DownloadItem* download = downloads[0];
  EXPECT_EQ(download::DownloadItem::INTERRUPTED, download->GetState());
  download->Resume(false);
  WaitForCompletion(download);
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    std::string partial_file;
    ASSERT_TRUE(
        base::ReadFileToString(download->GetTargetFilePath(), &partial_file));
    EXPECT_EQ(partial_file, TestDownloadHttpResponse::GetPatternBytes(
                                parameters.pattern_generator_seed, 10, 99991));
  }
}

}  // namespace content
