// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains download browser tests that are known to be runnable
// in a pure content context.  Over time tests should be migrated here.

#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <vector>

#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/test/mock_entropy_provider.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/download/public/common/download_danger_type.h"
#include "components/download/public/common/download_features.h"
#include "components/download/public/common/download_file_factory.h"
#include "components/download/public/common/download_file_impl.h"
#include "components/download/public/common/download_task_runner.h"
#include "components/download/public/common/parallel_download_configs.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/public/test/slow_download_http_response.h"
#include "content/public/test/test_download_http_response.h"
#include "content/public/test/test_file_error_injector.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_browser_context.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "content/shell/browser/shell_network_delegate.h"
#include "content/test/content_browser_test_utils_internal.h"
#include "content/test/test_content_browser_client.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "ppapi/buildflags/buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/platform/web_mouse_event.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/browser/plugin_service_impl.h"
#endif

using ::testing::AllOf;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Property;
using ::testing::Return;
using ::testing::StrictMock;
using ::testing::_;

namespace net {
class NetLogWithSource;
}

namespace content {

namespace {

// Default request count for parallel download tests.
constexpr int kTestRequestCount = 3;

const std::string kOriginOne = "one.example";
const std::string kOriginTwo = "two.example";
const std::string kOriginThree = "example.com";
const char k404Response[] = "HTTP/1.1 404 Not found\r\n\r\n";

// Implementation of TestContentBrowserClient that overrides
// AllowRenderingMhtmlOverHttp() and allows consumers to set a value.
class DownloadTestContentBrowserClient : public TestContentBrowserClient {
 public:
  DownloadTestContentBrowserClient() = default;

  bool AllowRenderingMhtmlOverHttp(NavigationUIData* navigation_data) override {
    return allowed_rendering_mhtml_over_http_;
  }

  void set_allowed_rendering_mhtml_over_http(bool allowed) {
    allowed_rendering_mhtml_over_http_ = allowed;
  }

  base::FilePath GetDefaultDownloadDirectory() override {
    return base::FilePath();
  }

 private:
  bool allowed_rendering_mhtml_over_http_ = false;

  DISALLOW_COPY_AND_ASSIGN(DownloadTestContentBrowserClient);
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
  MockDownloadManagerObserver(DownloadManager* manager) {
    manager_ = manager;
    manager->AddObserver(this);
  }
  ~MockDownloadManagerObserver() override {
    if (manager_)
      manager_->RemoveObserver(this);
  }

  MOCK_METHOD2(OnDownloadCreated,
               void(DownloadManager*, download::DownloadItem*));
  MOCK_METHOD1(ModelChanged, void(DownloadManager*));
  void ManagerGoingDown(DownloadManager* manager) override {
    DCHECK_EQ(manager_, manager);
    MockManagerGoingDown(manager);

    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }

  MOCK_METHOD1(MockManagerGoingDown, void(DownloadManager*));
 private:
  DownloadManager* manager_;
};

class DownloadFileWithDelayFactory;

static DownloadManagerImpl* DownloadManagerForShell(Shell* shell) {
  // We're in a content_browsertest; we know that the DownloadManager
  // is a DownloadManagerImpl.
  return static_cast<DownloadManagerImpl*>(
      BrowserContext::GetDownloadManager(
          shell->web_contents()->GetBrowserContext()));
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

  ~DownloadFileWithDelay() override;

  // Wraps DownloadFileImpl::Rename* and intercepts the return callback,
  // storing it in the factory that produced this object for later
  // retrieval.
  void RenameAndUniquify(const base::FilePath& full_path,
                         const RenameCompletionCallback& callback) override;
  void RenameAndAnnotate(const base::FilePath& full_path,
                         const std::string& client_guid,
                         const GURL& source_url,
                         const GURL& referrer_url,
                         const RenameCompletionCallback& callback) override;

 private:
  static void RenameCallbackWrapper(
      const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
      const RenameCompletionCallback& original_callback,
      download::DownloadInterruptReason reason,
      const base::FilePath& path);

  // This variable may only be read on the download sequence, and may only be
  // indirected through (e.g. methods on DownloadFileWithDelayFactory called)
  // on the UI thread.  This is because after construction,
  // DownloadFileWithDelay lives on the file thread, but
  // DownloadFileWithDelayFactory is purely a UI thread object.
  base::WeakPtr<DownloadFileWithDelayFactory> owner_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelay);
};

// All routines on this class must be called on the UI thread.
class DownloadFileWithDelayFactory : public download::DownloadFileFactory {
 public:
  DownloadFileWithDelayFactory();
  ~DownloadFileWithDelayFactory() override;

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
      base::WeakPtr<download::DownloadDestinationObserver> observer) override;

  void AddRenameCallback(base::Closure callback);
  void GetAllRenameCallbacks(std::vector<base::Closure>* results);

  // Do not return until GetAllRenameCallbacks() will return a non-empty list.
  void WaitForSomeCallback();

 private:
  std::vector<base::Closure> rename_callbacks_;
  base::OnceClosure stop_waiting_;
  base::WeakPtrFactory<DownloadFileWithDelayFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DownloadFileWithDelayFactory);
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
    const RenameCompletionCallback& callback) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  download::DownloadFileImpl::RenameAndUniquify(
      full_path, base::Bind(DownloadFileWithDelay::RenameCallbackWrapper,
                            owner_, callback));
}

void DownloadFileWithDelay::RenameAndAnnotate(
    const base::FilePath& full_path,
    const std::string& client_guid,
    const GURL& source_url,
    const GURL& referrer_url,
    const RenameCompletionCallback& callback) {
  DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
  download::DownloadFileImpl::RenameAndAnnotate(
      full_path, client_guid, source_url, referrer_url,
      base::Bind(DownloadFileWithDelay::RenameCallbackWrapper, owner_,
                 callback));
}

// static
void DownloadFileWithDelay::RenameCallbackWrapper(
    const base::WeakPtr<DownloadFileWithDelayFactory>& factory,
    const RenameCompletionCallback& original_callback,
    download::DownloadInterruptReason reason,
    const base::FilePath& path) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!factory)
    return;
  factory->AddRenameCallback(base::Bind(original_callback, reason, path));
}

DownloadFileWithDelayFactory::DownloadFileWithDelayFactory()
    : weak_ptr_factory_(this) {}

DownloadFileWithDelayFactory::~DownloadFileWithDelayFactory() {}

download::DownloadFile* DownloadFileWithDelayFactory::CreateFile(
    std::unique_ptr<download::DownloadSaveInfo> save_info,
    const base::FilePath& default_download_directory,
    std::unique_ptr<download::InputStream> stream,
    uint32_t download_id,
    base::WeakPtr<download::DownloadDestinationObserver> observer) {
  return new DownloadFileWithDelay(
      std::move(save_info), default_download_directory, std::move(stream),
      download_id, observer, weak_ptr_factory_.GetWeakPtr());
}

void DownloadFileWithDelayFactory::AddRenameCallback(base::Closure callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  rename_callbacks_.push_back(std::move(callback));
  if (stop_waiting_)
    std::move(stop_waiting_).Run();
}

void DownloadFileWithDelayFactory::GetAllRenameCallbacks(
    std::vector<base::Closure>* results) {
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

  void Initialize(InitializeCallback callback,
                  const CancelRequestCallback& cancel_request_callback,
                  const download::DownloadItem::ReceivedSlices& received_slices,
                  bool is_parallelizable) override {
    DCHECK(download::GetDownloadTaskRunner()->RunsTasksInCurrentSequence());
    active_files_++;
    download::DownloadFileImpl::Initialize(std::move(callback),
                                           cancel_request_callback,
                                           received_slices, is_parallelizable);
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
  ErrorInjectionDownloadFileFactory()
      : download_file_(nullptr), weak_ptr_factory_(this) {}
  ~ErrorInjectionDownloadFileFactory() override = default;

  // DownloadFileFactory interface.
  download::DownloadFile* CreateFile(
      std::unique_ptr<download::DownloadSaveInfo> save_info,
      const base::FilePath& default_download_directory,
      std::unique_ptr<download::InputStream> stream,
      uint32_t download_id,
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

  ErrorInjectionDownloadFile* download_file_;
  int64_t injected_error_offset_ = -1;
  int64_t injected_error_length_ = 0;
  base::WeakPtrFactory<ErrorInjectionDownloadFileFactory> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ErrorInjectionDownloadFileFactory);
};

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  TestShellDownloadManagerDelegate()
      : delay_download_open_(false) {}
  ~TestShellDownloadManagerDelegate() override {}

  bool ShouldOpenDownload(
      download::DownloadItem* item,
      const DownloadOpenDelayedCallback& callback) override {
    if (delay_download_open_) {
      delayed_callbacks_.push_back(callback);
      return false;
    }
    return true;
  }

  bool GenerateFileHash() override { return true; }

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
  DownloadCreateObserver(DownloadManager* manager)
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

    if (!completion_closure_.is_null())
      base::ResetAndReturn(&completion_closure_).Run();
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
  DownloadManager* manager_;
  download::DownloadItem* item_;
  base::Closure completion_closure_;
};

class ErrorStreamCountingObserver : download::DownloadItem::Observer {
 public:
  ErrorStreamCountingObserver() : item_(nullptr), count_(0){};

  ~ErrorStreamCountingObserver() override {
    if (item_)
      item_->RemoveObserver(this);
  }

  void OnDownloadUpdated(download::DownloadItem* download) override {
    std::unique_ptr<base::HistogramSamples> samples =
        histogram_tester_.GetHistogramSamplesSinceCreation(
            "Download.ParallelDownloadAddStreamSuccess");
    if (samples->GetCount(0 /* failure */) == count_ &&
        !completion_closure_.is_null())
      base::ResetAndReturn(&completion_closure_).Run();
  }

  void OnDownloadDestroyed(download::DownloadItem* download) override {
    item_ = nullptr;
  }

  void WaitForFinished(download::DownloadItem* item, int count) {
    item_ = item;
    count_ = count;
    if (item_) {
      item_->AddObserver(this);
      base::RunLoop run_loop;
      completion_closure_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

 private:
  base::HistogramTester histogram_tester_;
  download::DownloadItem* item_;
  int count_;
  base::Closure completion_closure_;
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
    response.reset(new net::test_server::BasicHttpResponse);
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
  return base::Bind(
      &HandleRequestAndSendRedirectResponse, relative_url, target_url);
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
    response.reset(new net::test_server::BasicHttpResponse);
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
  return base::Bind(&HandleRequestAndSendBasicResponse, relative_url, code,
                    headers, content_type, body);
}

std::unique_ptr<net::test_server::HttpResponse> HandleRequestAndEchoCookies(
    const std::string& relative_url,
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> response;
  if (request.relative_url == relative_url) {
    response.reset(new net::test_server::BasicHttpResponse);
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
    return base::Bind(&TestRequestPauseHandler::OnPauseHandler,
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
  void OnPauseHandler(const base::Closure& resume_callback) {
    resume_callback_ = resume_callback;
    if (run_loop_.running())
      run_loop_.Quit();
  }

  bool used_ = false;
  base::RunLoop run_loop_;
  base::OnceClosure resume_callback_;
};

class DownloadContentTest : public ContentBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(downloads_directory_.CreateUniqueTempDir());

    test_delegate_.reset(new TestShellDownloadManagerDelegate());
    test_delegate_->SetDownloadBehaviorForTesting(
        downloads_directory_.GetPath());
    DownloadManager* manager = DownloadManagerForShell(shell());
    manager->GetDelegate()->Shutdown();
    manager->SetDelegate(test_delegate_.get());
    test_delegate_->SetDownloadManager(manager);

    base::FilePath test_data_dir;
    ASSERT_TRUE(base::PathService::Get(content::DIR_TEST_DATA, &test_data_dir));
    embedded_test_server()->ServeFilesFromDirectory(test_data_dir);
    embedded_test_server()->RegisterRequestHandler(
        base::Bind(&SlowDownloadHttpResponse::HandleSlowDownloadRequest));
    test_response_handler_.RegisterToTestServer(embedded_test_server());
    ASSERT_TRUE(embedded_test_server()->Start());
    const std::string real_host =
        embedded_test_server()->host_port_pair().host();
    host_resolver()->AddRule(kOriginOne, real_host);
    host_resolver()->AddRule(kOriginTwo, real_host);
    host_resolver()->AddRule(kOriginThree, real_host);
    host_resolver()->AddRule(SlowDownloadHttpResponse::kSlowDownloadHostName,
                             real_host);
    host_resolver()->AddRule(TestDownloadHttpResponse::kTestDownloadHostName,
                             real_host);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    IsolateAllSitesForTesting(command_line);
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

  void WaitForInterrupt(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download,
        base::Bind(&IsDownloadInState, download::DownloadItem::INTERRUPTED))
        .WaitForEvent();
  }

  void WaitForInProgress(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download,
        base::Bind(&IsDownloadInState, download::DownloadItem::IN_PROGRESS))
        .WaitForEvent();
  }

  void WaitForCompletion(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download,
        base::Bind(&IsDownloadInState, download::DownloadItem::COMPLETE))
        .WaitForEvent();
  }

  void WaitForCancel(download::DownloadItem* download) {
    DownloadUpdatedObserver(
        download,
        base::Bind(&IsDownloadInState, download::DownloadItem::CANCELLED))
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
    inject_error_callback_ = base::Bind(
        &ErrorInjectionDownloadFileFactory::InjectError, factory->GetWeakPtr());

    DownloadManagerForShell(shell())->SetDownloadFileFactoryForTesting(
        std::move(factory));
  }

  void NavigateToURLAndWaitForDownload(
      Shell* shell,
      const GURL& url,
      download::DownloadItem::DownloadState expected_terminal_state) {
    std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell, 1));
    NavigateToURL(shell, url);
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
      int bytes_read = file.Read(offset, &data.front(), kBufferSize);
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
    NavigateToURL(
        shell, embedded_test_server()->GetURL("/register_service_worker.html"));
    EXPECT_EQ("DONE", EvalJs(shell, "register('" + worker_url + "')"));
  }

 private:
  // Location of the downloads directory for these tests
  base::ScopedTempDir downloads_directory_;
  std::unique_ptr<TestShellDownloadManagerDelegate> test_delegate_;
  TestDownloadResponseHandler test_response_handler_;
  TestDownloadHttpResponse::InjectErrorCallback inject_error_callback_;
};

// Test fixture for parallel downloading.
class ParallelDownloadTest : public DownloadContentTest {
 protected:
  ParallelDownloadTest() {
    std::map<std::string, std::string> params = {
        {download::kMinSliceSizeFinchKey, "1"},
        {download::kParallelRequestCountFinchKey,
         base::IntToString(kTestRequestCount)},
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
      TestDownloadHttpResponse::Parameters& parameters) {
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
          EXPECT_EQ(bytes_to_write,
                    file.Write(offset, output.data(), bytes_to_write));
          total_bytes += bytes_to_write;
          offset += bytes_to_write;
        }
      }
      file.Close();
    }

    download::DownloadItem* download =
        DownloadManagerForShell(shell())->CreateDownloadItem(
            "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, path, base::FilePath(),
            url_chain, GURL(), GURL(), GURL(), GURL(),
            "application/octet-stream", "application/octet-stream",
            base::Time::Now(), base::Time(), parameters.etag,
            parameters.last_modified, total_bytes, parameters.size,
            std::string(), download::DownloadItem::INTERRUPTED,
            download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
            download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
            base::Time(), false, slices);
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
    EXPECT_TRUE(
        base::FeatureList::IsEnabled(download::features::kParallelDownloading));
    GURL url = TestDownloadHttpResponse::GetNextURLForDownload();
    GURL server_url = embedded_test_server()->GetURL(url.host(), url.path());
    TestDownloadHttpResponse::Parameters parameters;
    parameters.etag = "ABC";
    parameters.size = total_length;
    parameters.last_modified = std::string();
    parameters.support_partial_response = support_partial_response;
    // Needed to specify HTTP connection type to create parallel download.
    parameters.connection_type = net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1;
    TestDownloadHttpResponse::StartServing(parameters, server_url);

    base::FilePath intermediate_file_path =
        GetDownloadDirectory().AppendASCII("intermediate");
    std::vector<GURL> url_chain;
    url_chain.push_back(server_url);

    // Create the intermediate file reflecting the received slices.
    download::DownloadItem* download = CreateDownloadAndIntermediateFile(
        intermediate_file_path, url_chain, received_slices, parameters);

    // Resume the parallel download with sparse file and received slices data.
    download->Resume();
    WaitForCompletion(download);
    // TODO(qinmin): count the failed partial responses in DownloadJob when
    // support_partial_response is false. EmbeddedTestServer doesn't know
    // whether completing or canceling the response will come first.
    if (support_partial_response) {
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

  // Verifies parallel download completion.
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
    parameters.connection_type = net::HttpResponseInfo::CONNECTION_INFO_HTTP1_1;
    TestRequestPauseHandler request_pause_handler;
    parameters.on_pause_handler = request_pause_handler.GetOnPauseHandler();
    // Send some data for the first request and pause it so download won't
    // complete before other parallel requests are created.
    parameters.pause_offset = DownloadRequestCore::kDownloadByteStreamSize;
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

  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadTest);
};

}  // namespace

// Flaky. See https://crbug.com/754679.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadCancelled) {
  SetupEnsureNoPendingDownloads();

  // Create a download, wait until it's started, and confirm
  // we're in the expected state.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   SlowDownloadHttpResponse::kSlowDownloadHostName,
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
                   SlowDownloadHttpResponse::kSlowDownloadHostName,
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
  NavigateToURL(shell(), embedded_test_server()->GetURL(
                             SlowDownloadHttpResponse::kSlowDownloadHostName,
                             SlowDownloadHttpResponse::kFinishDownloadUrl));
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
  size_t file_size1 = SlowDownloadHttpResponse::kFirstDownloadSize +
                      SlowDownloadHttpResponse::kSecondDownloadSize;
  std::string expected_contents(file_size1, '*');
  ASSERT_TRUE(VerifyFile(file1, expected_contents, file_size1));

  base::FilePath file2(download2->GetTargetFilePath());
  ASSERT_TRUE(base::ContentsEqual(
      file2, GetTestFilePath("download", "download-test.lib")));
}

#if BUILDFLAG(ENABLE_PLUGINS)
// Content served with a MIME type of application/octet-stream should be
// downloaded even when a plugin can be found that handles the file type.
// See https://crbug.com/104331 for the details.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadOctetStream) {
  const char kTestPluginName[] = "TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  WebPluginInfo plugin_info;
  plugin_info.name = base::ASCIIToUTF16(kTestPluginName);
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
  const char kTestPluginName[] = "TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  RegisterServiceWorker(shell(), "/fetch_event_passthrough.js");

  WebPluginInfo plugin_info;
  plugin_info.name = base::ASCIIToUTF16(kTestPluginName);
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
  const char kTestPluginName[] = "TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  RegisterServiceWorker(shell(), "/fetch_event_octet_stream.js");

  WebPluginInfo plugin_info;
  plugin_info.name = base::ASCIIToUTF16(kTestPluginName);
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
  const char kTestPluginName[] = "TestPlugin";
  const char kTestMimeType[] = "application/x-test-mime-type";
  const char kTestFileType[] = "abc";

  RegisterServiceWorker(shell(), "/fetch_event_respond_with_fetch.js");

  WebPluginInfo plugin_info;
  plugin_info.name = base::ASCIIToUTF16(kTestPluginName);
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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/download/download-test.lib"));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Cancel it.
  std::vector<download::DownloadItem*> items;
  download_manager->GetAllDownloads(&items);
  ASSERT_EQ(1u, items.size());
  items[0]->Cancel(true);
  RunAllTasksUntilIdle();

  // Check state.
  EXPECT_EQ(download::DownloadItem::CANCELLED, items[0]->GetState());

  // Run final rename callback.
  callbacks[0].Run();
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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/download/download-test.lib"));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  callbacks[0].Run();
  callbacks.clear();

  // Confirm download still IN_PROGRESS (internal state COMPLETING).
  std::vector<download::DownloadItem*> items;
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
  delayed_callbacks[0].Run(true);

  // *Now* the download should be complete.
  EXPECT_EQ(download::DownloadItem::COMPLETE, items[0]->GetState());
}

// Try to shutdown with a download in progress to make sure shutdown path
// works properly.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, ShutdownInProgress) {
  // Create a download that won't complete.
  download::DownloadItem* download = StartDownloadAndReturnItem(
      shell(), embedded_test_server()->GetURL(
                   SlowDownloadHttpResponse::kSlowDownloadHostName,
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

  // See http://crbug.com/324525.  If we have a refcount release/post task
  // race, the second post will stall the IO thread long enough so that we'll
  // lose the race and crash.  The first stall is just to give the UI thread
  // a chance to get the second stall onto the IO thread queue after the cancel
  // message created by Shutdown and before the notification callback
  // created by the IO thread in canceling the request.
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&base::PlatformThread::Sleep,
                     base::TimeDelta::FromMilliseconds(25)));
  DownloadManagerForShell(shell())->Shutdown();
  base::PostTaskWithTraits(
      FROM_HERE, {BrowserThread::IO},
      base::BindOnce(&base::PlatformThread::Sleep,
                     base::TimeDelta::FromMilliseconds(25)));
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
  NavigateToURL(shell(),
                embedded_test_server()->GetURL("/download/download-test.lib"));

  // Wait until the first (intermediate file) rename and execute the callback.
  file_factory->WaitForSomeCallback();
  std::vector<base::Closure> callbacks;
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());
  callbacks[0].Run();
  callbacks.clear();

  // Wait until the second (final) rename callback is posted.
  file_factory->WaitForSomeCallback();
  file_factory->GetAllRenameCallbacks(&callbacks);
  ASSERT_EQ(1u, callbacks.size());

  // Call it.
  callbacks[0].Run();
  callbacks.clear();

  // Confirm download isn't complete yet.
  std::vector<download::DownloadItem*> items;
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
  EXPECT_CALL(observer, OnDownloadDestroyed(items[0]));

  // Shutdown the download manager.  Mostly this is confirming a lack of
  // crashes.
  DownloadManagerForShell(shell())->Shutdown();
}

// Test resumption with a response that contains strong validators.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, StrongValidators) {
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

  download->Resume();
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
  ASSERT_TRUE(requests[1]->http_request.headers.find(
                  net::HttpRequestHeaders::kIfRange) !=
              requests[1]->http_request.headers.end());
  EXPECT_EQ(parameters.etag, requests[1]->http_request.headers.at(
                                 net::HttpRequestHeaders::kIfRange));

  ASSERT_TRUE(
      requests[1]->http_request.headers.find(net::HttpRequestHeaders::kRange) !=
      requests[1]->http_request.headers.end());
  EXPECT_EQ(
      base::StringPrintf("bytes=%" PRId64 "-", interruption_offset),
      requests[1]->http_request.headers.at(net::HttpRequestHeaders::kRange));
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

  download->Resume();
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
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_UNREACHABLE,
            download->GetLastReason());

  // Back to the original request handler. Resumption should now succeed, and
  // use the partial data it had prior to the first interruption.
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, first_url);
  download->Resume();
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

// If the server response for the resumption request specifies a bad range (i.e.
// not the range that was requested or an invalid or missing Content-Range
// header), then the download should be marked as interrupted again without
// discarding the partial state.
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
  TestDownloadHttpResponse::StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: bytes 1000000-2000000/3000000\r\n"
      "\r\n",
      server_url);
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Or this time, the server sends a response with an invalid Content-Range
  // header.
  TestDownloadHttpResponse::StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Content-Range: ooga-booga-booga-booga\r\n"
      "\r\n",
      server_url);
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Or no Content-Range header at all.
  TestDownloadHttpResponse::StartServingStaticResponse(
      "HTTP/1.1 206 Partial Content\r\n"
      "Some-Headers: ooga-booga-booga-booga\r\n"
      "\r\n",
      server_url);
  download->Resume();
  WaitForInterrupt(download);
  EXPECT_EQ(download::DOWNLOAD_INTERRUPT_REASON_SERVER_BAD_CONTENT,
            download->GetLastReason());

  // Back to the original request handler. Resumption should now succeed, and
  // use the partial data it had prior to the first interruption.
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);
  download->Resume();
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

  ASSERT_EQ(5u, requests.size());

  // None of the request should have transferred the entire resource.
  EXPECT_GT(parameters.size, requests[0]->transferred_byte_count);
  EXPECT_EQ(0, requests[1]->transferred_byte_count);
  EXPECT_EQ(0, requests[2]->transferred_byte_count);
  EXPECT_EQ(0, requests[3]->transferred_byte_count);
  EXPECT_GT(parameters.size, requests[4]->transferred_byte_count);
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

  download->Resume();
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

  ASSERT_TRUE(requests[1]->http_request.headers.find(
                  net::HttpRequestHeaders::kIfRange) !=
              requests[1]->http_request.headers.end());
  EXPECT_EQ(parameters.etag, requests[1]->http_request.headers.at(
                                 net::HttpRequestHeaders::kIfRange));

  ASSERT_TRUE(
      requests[1]->http_request.headers.find(net::HttpRequestHeaders::kRange) !=
      requests[1]->http_request.headers.end());
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

  download->Resume();
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
  EXPECT_TRUE(requests[1]->http_request.headers.find(
                  net::HttpRequestHeaders::kIfRange) ==
              requests[1]->http_request.headers.end());
  EXPECT_TRUE(
      requests[1]->http_request.headers.find(net::HttpRequestHeaders::kRange) ==
      requests[1]->http_request.headers.end());
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
    ASSERT_TRUE(base::DeleteFile(download->GetFullPath(), false));
  }

  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume();
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
  download->Resume();
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

  download->Resume();
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

  download->Resume();
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
  download->Resume();
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume();
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume();
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume();
  WaitForInterrupt(download);

  parameters.injected_errors.pop();
  TestDownloadHttpResponse::StartServing(parameters, server_url2);
  download->Resume();
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

  download->Resume();
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
  EXPECT_CALL(dm_observer, OnDownloadCreated(_,_)).Times(1);

  TestRequestPauseHandler request_pause_handler;
  parameters.on_pause_handler = request_pause_handler.GetOnPauseHandler();
  parameters.pause_offset = -1;
  parameters.ClearInjectedErrors();
  TestDownloadHttpResponse::StartServing(parameters, server_url);

  download->Resume();
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

// Flaky on ASAN. crbug.com/838403
#if defined(ADDRESS_SANITIZER)
#define MAYBE_RemoveResumedDownload DISABLED_RemoveResumedDownload
#else
#define MAYBE_RemoveResumedDownload RemoveResumedDownload
#endif  // defined(ADDRESS_SANITIZER)

IN_PROC_BROWSER_TEST_F(DownloadContentTest, MAYBE_RemoveResumedDownload) {
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

  download->Resume();
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
#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
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

  download->Resume();
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
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), parameters.etag, std::string(),
          kIntermediateSize, parameters.size, std::string(),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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
    ASSERT_EQ(kIntermediateSize, base::WriteFile(intermediate_file_path,
                                                 output.data(), output.size()));
  }

  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), parameters.etag, std::string(),
          kIntermediateSize, parameters.size, std::string(),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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
    ASSERT_EQ(kIntermediateSize, base::WriteFile(intermediate_file_path,
                                                 output.data(), output.size()));
  }

  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), "fake-etag", std::string(),
          kIntermediateSize, parameters.size, std::string(),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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
    ASSERT_EQ(kIntermediateSize, base::WriteFile(intermediate_file_path,
                                                 output.data(), output.size()));
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
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), parameters.etag, std::string(),
          kIntermediateSize, parameters.size,
          std::string(std::begin(kPartialHash), std::end(kPartialHash)),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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
    ASSERT_EQ(kIntermediateSize, base::WriteFile(intermediate_file_path,
                                                 buffer.data(), buffer.size()));
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
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), parameters.etag, std::string(),
          kIntermediateSize, parameters.size,
          std::string(std::begin(kPartialHash), std::end(kPartialHash)),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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
    ASSERT_EQ(
        kIntermediateSize - 100,
        base::WriteFile(intermediate_file_path, output.data(), output.size()));
  }
  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), parameters.etag, std::string(),
          kIntermediateSize, parameters.size, std::string(),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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
    ASSERT_EQ(
        kIntermediateSize + 100,
        base::WriteFile(intermediate_file_path, output.data(), output.size()));
  }
  url_chain.push_back(server_url);

  download::DownloadItem* download =
      DownloadManagerForShell(shell())->CreateDownloadItem(
          "F7FB1F59-7DE1-4845-AFDB-8A688F70F583", 1, intermediate_file_path,
          base::FilePath(), url_chain, GURL(), GURL(), GURL(), GURL(),
          "application/octet-stream", "application/octet-stream",
          base::Time::Now(), base::Time(), parameters.etag, std::string(),
          kIntermediateSize, parameters.size, std::string(),
          download::DownloadItem::INTERRUPTED,
          download::DOWNLOAD_DANGER_TYPE_NOT_DANGEROUS,
          download::DOWNLOAD_INTERRUPT_REASON_NETWORK_FAILED, false,
          base::Time(), false,
          std::vector<download::DownloadItem::ReceivedSlice>());

  download->Resume();
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

  download->Resume();
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
  EXPECT_TRUE(last_request.headers.find(net::HttpRequestHeaders::kReferer) !=
              last_request.headers.end());
  EXPECT_EQ(document_url.spec(),
            last_request.headers.at(net::HttpRequestHeaders::kReferer));
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

// Check that the cookie policy is correctly updated when downloading a file
// that redirects cross origin.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, CookiePolicy) {
  net::EmbeddedTestServer origin_one;
  net::EmbeddedTestServer origin_two;

  // Block third-party cookies.
  ShellNetworkDelegate::SetBlockThirdPartyCookies(true);

  // |url| redirects to a different origin |download| which tries to set a
  // cookie.
  base::StringPairs cookie_header;
  cookie_header.push_back(
      std::make_pair(std::string("Set-Cookie"), std::string("A=B")));
  origin_one.RegisterRequestHandler(CreateBasicResponseHandler(
      "/foo", net::HTTP_OK, cookie_header, "application/octet-stream", "abcd"));
  ASSERT_TRUE(origin_one.Start());

  origin_two.RegisterRequestHandler(
      CreateRedirectHandler("/bar", origin_one.GetURL("/foo")));
  ASSERT_TRUE(origin_two.Start());

  // Download the file.
  SetupEnsureNoPendingDownloads();
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), origin_two.GetURL("/bar"),
          TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  // Get the important info from other threads and check it.
  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());

  // Check that the cookies were correctly set.
  EXPECT_EQ("A=B",
            content::GetCookies(shell()->web_contents()->GetBrowserContext(),
                                origin_one.GetURL("/")));
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

  // <origin_one>/download-attribute.html initiates a download of
  // <origin_one>/ping, which redirects to <origin_two>/download.
  origin_one.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  origin_one.RegisterRequestHandler(
      CreateRedirectHandler("/ping", origin_two.GetURL("/download")));
  origin_one.StartAcceptingConnections();

  origin_two.RegisterRequestHandler(
      CreateBasicResponseHandler("/download", net::HTTP_OK, base::StringPairs(),
                                 "application/octet-stream", "Hello"));
  origin_two.StartAcceptingConnections();

  NavigateToURLAndWaitForDownload(shell(), referrer_url,
                                  download::DownloadItem::COMPLETE);

  std::vector<download::DownloadItem*> downloads;
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

  NavigateToURLAndWaitForDownload(shell(), referrer_url,
                                  download::DownloadItem::COMPLETE);

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("download"),
            downloads[0]->GetTargetFilePath().BaseName().value());
  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A file type that Blink can handle should not be downloaded if there are cross
// origin redirects in the middle of the redirect chain.
IN_PROC_BROWSER_TEST_F(DownloadContentTest,
                       DownloadAttributeSameOriginRedirectNavigation) {
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

  base::string16 expected_title(base::UTF8ToUTF16("hello"));
  TitleWatcher observer(shell()->web_contents(), expected_title);
  NavigateToURL(shell(), referrer_url);
  ASSERT_EQ(expected_title, observer.WaitAndGetTitle());

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(0u, downloads.size());

  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}

// A download initiated by the user via alt-click on a link should download,
// even when redirected cross origin.
//
// Alt-click doesn't make sense on Android, and download a HTML file results
// in an intent, so just skip.
#if !defined(OS_ANDROID)
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
  NavigateToURL(shell(), referrer_url);

  // Alt-click the link.
  blink::WebMouseEvent mouse_event(
      blink::WebInputEvent::kMouseDown, blink::WebInputEvent::kAltKey,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.button = blink::WebMouseEvent::Button::kLeft;
  mouse_event.SetPositionInWidget(15, 15);
  mouse_event.click_count = 1;
  shell()->web_contents()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);
  mouse_event.SetType(blink::WebInputEvent::kMouseUp);
  shell()->web_contents()->GetRenderViewHost()->GetWidget()->ForwardMouseEvent(
      mouse_event);

  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
#if defined(OS_WIN)
  EXPECT_EQ(FILE_PATH_LITERAL("download.htm"),
            downloads[0]->GetTargetFilePath().BaseName().value());
#else
  EXPECT_EQ(FILE_PATH_LITERAL("download.html"),
            downloads[0]->GetTargetFilePath().BaseName().value());
#endif

  ASSERT_TRUE(origin_one.ShutdownAndWaitUntilComplete());
  ASSERT_TRUE(origin_two.ShutdownAndWaitUntilComplete());
}
#endif  // !defined(OS_ANDROID)

// Test that the suggested filename for data: URLs works.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeDataUrl) {
  net::EmbeddedTestServer server;
  ASSERT_TRUE(server.InitializeAndListen());

  GURL url = server.GetURL(std::string(
      "/download-attribute.html?target=data:application/octet-stream, ..."));
  server.ServeFilesFromDirectory(GetTestFilePath("download", ""));
  server.StartAcceptingConnections();

  NavigateToURLAndWaitForDownload(shell(), url,
                                  download::DownloadItem::COMPLETE);

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("suggested-filename"),
            downloads[0]->GetTargetFilePath().BaseName().value());
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
  navigation_document.WaitForNavigationFinished();
  navigation_download.WaitForNavigationFinished();

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
  auto observer = std::make_unique<content::TestNavigationObserver>(
      GURL(url::kAboutBlankURL));
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();
  NavigateToURL(shell(), url);
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

class DownloadContentTestWithMojoBlobURLs : public DownloadContentTest {
 public:
  DownloadContentTestWithMojoBlobURLs() {
    scoped_feature_list_.InitAndEnableFeature(blink::features::kMojoBlobURLs);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DownloadContentTestWithMojoBlobURLs,
                       DownloadAttributeBlobURL) {
  GURL document_url =
      embedded_test_server()->GetURL("/download/download-attribute-blob.html");
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), document_url);
  WaitForCompletion(download);

  EXPECT_STREQ(FILE_PATH_LITERAL("suggested-filename.txt"),
               download->GetTargetFilePath().BaseName().value().c_str());
}

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadAttributeSameSiteCookie) {
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

  // OriginOne redirects through OriginTwo.
  //
  //  Initiator origin: kOriginOne
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
  EXPECT_STREQ("A=B; B=C", file_contents.c_str());
}

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

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DuplicateContentDisposition) {
  // double-content-disposition.txt is served with two Content-Disposition
  // headers, both of which are identical.
  NavigateToURLAndWaitForDownload(
      shell(),
      embedded_test_server()->GetURL(
          "/download/double-content-disposition.txt"),
      download::DownloadItem::COMPLETE);

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());

  EXPECT_EQ(FILE_PATH_LITERAL("Jumboshrimp.txt"),
            downloads[0]->GetTargetFilePath().BaseName().value());
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

  GURL frame_url =
      origin_one.GetURL("/download-attribute.html?target=" +
                        origin_two.GetURL("/download-test.lib").spec());
  GURL::Replacements replacements;
  replacements.SetHostStr("localhost");
  frame_url = frame_url.ReplaceComponents(replacements);
  GURL document_url =
      origin_two.GetURL("/iframe-host.html?target=" + frame_url.spec());
  download::DownloadItem* download =
      StartDownloadAndReturnItem(shell(), document_url);
  WaitForCompletion(download);

  EXPECT_STREQ(FILE_PATH_LITERAL("download-test.lib"),
               download->GetTargetFilePath().BaseName().value().c_str());
}

#if defined(OS_WIN)
// Flaky on windows: https://crbug.com/810982
#define MAYBE_ParallelDownloadComplete DISABLED_ParallelDownloadComplete
#else
#define MAYBE_ParallelDownloadComplete ParallelDownloadComplete
#endif
// Verify parallel download in normal case.
IN_PROC_BROWSER_TEST_F(ParallelDownloadTest, MAYBE_ParallelDownloadComplete) {
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

// Test to verify that the browser-side enforcement of X-Frame-Options does
// not impact downloads. Since XFO is only checked for subframes, this test
// initiates a download in an iframe and expects it to succeed.
// See https://crbug.com/717971.
IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadIgnoresXFO) {
  GURL main_url(
      embedded_test_server()->GetURL("/cross_site_iframe_factory.html?a(b)"));
  GURL download_url(
      embedded_test_server()->GetURL("/download/download-with-xfo-deny.html"));
  WebContentsImpl* web_contents =
      static_cast<WebContentsImpl*>(shell()->web_contents());

  EXPECT_TRUE(NavigateToURL(shell(), main_url));

  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  NavigateFrameToURL(web_contents->GetFrameTree()->root()->child_at(0),
                     download_url);
  observer->WaitForFinished();
  EXPECT_EQ(
      1u, observer->NumDownloadsSeenInState(download::DownloadItem::COMPLETE));

  std::vector<download::DownloadItem*> downloads;
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
  std::vector<download::DownloadItem*> items;
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
  std::vector<download::DownloadItem*> items;
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

  std::unique_ptr<DownloadTestObserver> observer;
  observer.reset(new content::DownloadTestObserverInterrupted(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_FAIL));
  download_manager->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();
  std::vector<download::DownloadItem*> items;
  download_manager->GetAllDownloads(&items);
  EXPECT_EQ(1u, items.size());

  // Now server will start to response 404 with empty body.
  TestDownloadHttpResponse::StartServingStaticResponse(k404Response,
                                                       server_url);
  download::DownloadItem* download = items[0];

  // The fetch error body should be cached in download item. The download should
  // start from beginning.
  download->Resume();
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

IN_PROC_BROWSER_TEST_F(DownloadContentTest, DownloadFromWebUI) {
  GURL webui_url("chrome://resources/images/apps/blue_button.png");
  NavigateToURL(shell(), webui_url);
  SetupEnsureNoPendingDownloads();
  std::unique_ptr<download::DownloadUrlParameters> download_parameters(
      DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          shell()->web_contents(), webui_url, TRAFFIC_ANNOTATION_FOR_TESTS));
  std::unique_ptr<DownloadTestObserver> observer(CreateWaiter(shell(), 1));
  DownloadManagerForShell(shell())->DownloadUrl(std::move(download_parameters));
  observer->WaitForFinished();

  EXPECT_TRUE(EnsureNoPendingDownloads());

  std::vector<download::DownloadItem*> downloads;
  DownloadManagerForShell(shell())->GetAllDownloads(&downloads);
  ASSERT_EQ(1u, downloads.size());
  ASSERT_EQ(download::DownloadItem::COMPLETE, downloads[0]->GetState());
}

// Test fixture for forcing MHTML download.
class MhtmlDownloadTest : public DownloadContentTest {
 protected:
  void SetUpOnMainThread() override {
    DownloadContentTest::SetUpOnMainThread();

    // Force downloading the MHTML.
    new_client_.set_allowed_rendering_mhtml_over_http(false);
    old_client_ = SetBrowserClientForTesting(&new_client_);
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(old_client_);
    DownloadContentTest::TearDownOnMainThread();
  }

 private:
  DownloadTestContentBrowserClient new_client_;
  ContentBrowserClient* old_client_;
};

IN_PROC_BROWSER_TEST_F(MhtmlDownloadTest, ForceDownloadMultipartRelatedPage) {
  NavigateToURLAndWaitForDownload(
      shell(),
      // .mhtml file is mapped to "multipart/related" by the test server.
      embedded_test_server()->GetURL("/download/hello.mhtml"),
      download::DownloadItem::COMPLETE);
}

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(ADDRESS_SANITIZER)
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
  void SetUpOnMainThread() override {
    DownloadContentTest::SetUpOnMainThread();

    // Allows loading the MHTML, instead of downloading it.
    new_client_.set_allowed_rendering_mhtml_over_http(true);
    old_client_ = SetBrowserClientForTesting(&new_client_);
  }

  void TearDownOnMainThread() override {
    SetBrowserClientForTesting(old_client_);
    DownloadContentTest::TearDownOnMainThread();
  }

 private:
  DownloadTestContentBrowserClient new_client_;
  ContentBrowserClient* old_client_;
};

IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest, AllowRenderMultipartRelatedPage) {
  // .mhtml file is mapped to "multipart/related" by the test server.
  GURL url = embedded_test_server()->GetURL("/download/hello.mhtml");
  auto observer = std::make_unique<content::TestNavigationObserver>(url);
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();

  NavigateToURL(shell(), url);

  observer->WaitForNavigationFinished();
}

IN_PROC_BROWSER_TEST_F(MhtmlLoadingTest, AllowRenderMessageRfc822Page) {
  // .mht file is mapped to "message/rfc822" by the test server.
  GURL url = embedded_test_server()->GetURL("/download/test.mht");
  auto observer = std::make_unique<content::TestNavigationObserver>(url);
  observer->WatchExistingWebContents();
  observer->StartWatchingNewWebContents();

  NavigateToURL(shell(), url);

  observer->WaitForNavigationFinished();
}

}  // namespace content
