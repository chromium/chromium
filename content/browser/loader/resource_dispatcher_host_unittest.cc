// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/unguessable_token.h"
#include "content/browser/child_process_security_policy_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/download_resource_handler.h"
#include "content/browser/frame_host/navigation_request_info.h"
#include "content/browser/loader/detachable_resource_handler.h"
#include "content/browser/loader/navigation_url_loader.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/loader/resource_loader.h"
#include "content/browser/loader/resource_message_filter.h"
#include "content/browser/loader/resource_request_info_impl.h"
#include "content/browser/loader_delegate_impl.h"
#include "content/common/appcache_interfaces.h"
#include "content/common/child_process_host_impl.h"
#include "content/common/navigation_params.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_request_id.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/resource_context.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/resource_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/browser_side_navigation_policy.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_features.h"
#include "content/public/common/process_type.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/test/test_content_browser_client.h"
#include "content/test/test_navigation_url_loader_delegate.h"
#include "mojo/public/cpp/system/data_pipe_utils.h"
#include "net/base/chunked_upload_data_stream.h"
#include "net/base/completion_once_callback.h"
#include "net/base/elements_upload_data_stream.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/base/upload_bytes_element_reader.h"
#include "net/http/http_util.h"
#include "net/test/cert_test_util.h"
#include "net/test/test_data_directory.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_job_factory.h"
#include "net/url_request/url_request_simple_job.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/resource_scheduler.h"
#include "services/network/resource_scheduler_params_manager.h"
#include "services/network/test/test_url_loader_client.h"
#include "storage/browser/blob/shareable_file_reference.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/page/page_visibility_state.mojom.h"

// TODO(eroman): Write unit tests for SafeBrowsing that exercise
//               SafeBrowsingResourceHandler.

using storage::ShareableFileReference;

namespace content {

static network::ResourceRequest CreateResourceRequest(const char* method,
                                                      ResourceType type,
                                                      const GURL& url) {
  network::ResourceRequest request;
  request.method = std::string(method);
  request.url = url;
  request.site_for_cookies = url;  // bypass third-party cookie blocking
  request.request_initiator =
      url::Origin::Create(url);  // ensure initiator is set
  request.referrer_policy = Referrer::GetDefaultReferrerPolicy();
  request.load_flags = 0;
  request.plugin_child_id = -1;
  request.resource_type = type;
  request.appcache_host_id = kAppCacheNoHostId;
  request.should_reset_appcache = false;
  request.render_frame_id = 0;
  request.is_main_frame = true;
  request.transition_type = ui::PAGE_TRANSITION_LINK;
  request.allow_download = true;
  request.keepalive = (type == RESOURCE_TYPE_PING);
  return request;
}

// This is used to create a filter matching a specified child id.
class TestFilterSpecifyingChild : public ResourceMessageFilter {
 public:
  TestFilterSpecifyingChild(BrowserContext* browser_context, int process_id)
      : ResourceMessageFilter(
            process_id,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            BrowserContext::GetSharedCorsOriginAccessList(browser_context),
            base::Bind(&TestFilterSpecifyingChild::GetContexts,
                       base::Unretained(this)),
            base::CreateSingleThreadTaskRunnerWithTraits({BrowserThread::IO})),
        resource_context_(browser_context->GetResourceContext()) {
    InitializeForTest();
    set_peer_process_for_testing(base::Process::Current());
  }

  // ResourceMessageFilter override
  bool Send(IPC::Message* msg) override {
    NOTREACHED();
    return true;
  }

  ResourceContext* resource_context() { return resource_context_; }

 protected:
  ~TestFilterSpecifyingChild() override {}

 private:
  void GetContexts(ResourceType resource_type,
                   ResourceContext** resource_context,
                   net::URLRequestContext** request_context) {
    *resource_context = resource_context_;
    *request_context = resource_context_->GetRequestContext();
  }

  ResourceContext* resource_context_;

  DISALLOW_COPY_AND_ASSIGN(TestFilterSpecifyingChild);
};

class TestFilter : public TestFilterSpecifyingChild {
 public:
  explicit TestFilter(BrowserContext* browser_context)
      : TestFilterSpecifyingChild(
            browser_context,
            ChildProcessHostImpl::GenerateChildProcessUniqueId()) {
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(child_id());
  }

 protected:
  ~TestFilter() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(child_id());
  }
};

// This class is a variation on URLRequestTestJob in that it does
// not complete start upon entry, only when specifically told to.
class URLRequestTestDelayedStartJob : public net::URLRequestTestJob {
 public:
  URLRequestTestDelayedStartJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate) {
    Init();
  }
  URLRequestTestDelayedStartJob(net::URLRequest* request,
                                net::NetworkDelegate* network_delegate,
                                const std::string& response_headers,
                                const std::string& response_data,
                                bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {
    Init();
  }

  // Do nothing until you're told to.
  void Start() override {}

  // Finish starting a URL request whose job is an instance of
  // URLRequestTestDelayedStartJob.  It is illegal to call this routine
  // with a URLRequest that does not use URLRequestTestDelayedStartJob.
  static void CompleteStart(net::URLRequest* request) {
    for (URLRequestTestDelayedStartJob* job = list_head_;
         job;
         job = job->next_) {
      if (job->request() == request) {
        job->net::URLRequestTestJob::Start();
        return;
      }
    }
    NOTREACHED();
  }

  static bool DelayedStartQueueEmpty() {
    return !list_head_;
  }

  static void ClearQueue() {
    if (list_head_) {
      LOG(ERROR)
          << "Unreleased entries on URLRequestTestDelayedStartJob delay queue"
          << "; may result in leaks.";
      list_head_ = nullptr;
    }
  }

 protected:
  ~URLRequestTestDelayedStartJob() override {
    for (URLRequestTestDelayedStartJob** job = &list_head_; *job;
         job = &(*job)->next_) {
      if (*job == this) {
        *job = (*job)->next_;
        return;
      }
    }
    NOTREACHED();
  }

 private:
  void Init() {
    next_ = list_head_;
    list_head_ = this;
  }

  static URLRequestTestDelayedStartJob* list_head_;
  URLRequestTestDelayedStartJob* next_;
};

URLRequestTestDelayedStartJob* URLRequestTestDelayedStartJob::list_head_ =
    nullptr;

// This class is a variation on URLRequestTestJob in that it
// returns IO_pending errors before every read, not just the first one.
class URLRequestTestDelayedCompletionJob : public net::URLRequestTestJob {
 public:
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate)
      : net::URLRequestTestJob(request, network_delegate) {}
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     bool auto_advance)
      : net::URLRequestTestJob(request, network_delegate, auto_advance) {}
  URLRequestTestDelayedCompletionJob(net::URLRequest* request,
                                     net::NetworkDelegate* network_delegate,
                                     const std::string& response_headers,
                                     const std::string& response_data,
                                     bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

 protected:
  ~URLRequestTestDelayedCompletionJob() override {}

 private:
  bool NextReadAsync() override { return true; }
};

// This class is a variation on URLRequestTestJob that ensures that no read is
// asked of this job.
class URLRequestMustNotReadTestJob : public net::URLRequestTestJob {
 public:
  URLRequestMustNotReadTestJob(net::URLRequest* request,
                               net::NetworkDelegate* network_delegate,
                               const std::string& response_headers,
                               const std::string& response_data)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               false) {}

  int ReadRawData(net::IOBuffer* buf, int buf_size) override {
    EXPECT_TRUE(false) << "The job should have been cancelled before trying to "
                          "read the response body.";
    return 0;
  }
};

class URLRequestBigJob : public net::URLRequestSimpleJob {
 public:
  URLRequestBigJob(net::URLRequest* request,
                   net::NetworkDelegate* network_delegate)
      : net::URLRequestSimpleJob(request, network_delegate) {
  }

  // URLRequestSimpleJob implementation:
  int GetData(std::string* mime_type,
              std::string* charset,
              std::string* data,
              net::CompletionOnceCallback callback) const override {
    *mime_type = "text/plain";
    *charset = "UTF-8";

    std::string text;
    int count;
    if (!ParseURL(request_->url(), &text, &count))
      return net::ERR_INVALID_URL;

    data->reserve(text.size() * count);
    for (int i = 0; i < count; ++i)
      data->append(text);

    return net::OK;
  }

 private:
  ~URLRequestBigJob() override {}

  // big-job:substring,N
  static bool ParseURL(const GURL& url, std::string* text, int* count) {
    std::vector<std::string> parts = base::SplitString(
        url.path(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (parts.size() != 2)
      return false;

    *text = parts[0];
    return base::StringToInt(parts[1], count);
  }
};

// URLRequestJob used to test GetLoadInfoForAllRoutes.  The LoadState and
// UploadProgress values are set for the jobs at the time of creation, and
// the jobs will never actually do anything.
class URLRequestLoadInfoJob : public net::URLRequestJob {
 public:
  URLRequestLoadInfoJob(net::URLRequest* request,
                        net::NetworkDelegate* network_delegate,
                        const net::LoadState& load_state)
      : net::URLRequestJob(request, network_delegate),
        load_state_(load_state) {}

  // net::URLRequestJob implementation:
  void Start() override {}
  net::LoadState GetLoadState() const override { return load_state_; }

 private:
  ~URLRequestLoadInfoJob() override {}

  // big-job:substring,N
  static bool ParseURL(const GURL& url, std::string* text, int* count) {
    std::vector<std::string> parts = base::SplitString(
        url.path(), ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

    if (parts.size() != 2)
      return false;

    *text = parts[0];
    return base::StringToInt(parts[1], count);
  }

  const net::LoadState load_state_;
};

class ResourceDispatcherHostTest;

class TestURLRequestJobFactory : public net::URLRequestJobFactory {
 public:
  explicit TestURLRequestJobFactory(ResourceDispatcherHostTest* test_fixture)
      : test_fixture_(test_fixture),
        hang_after_start_(false),
        delay_start_(false),
        delay_complete_(false),
        must_not_read_(false),
        url_request_jobs_created_count_(0) {}

  void HandleScheme(const std::string& scheme) {
    supported_schemes_.insert(scheme);
  }

  int url_request_jobs_created_count() const {
    return url_request_jobs_created_count_;
  }

  // When set, jobs will hang eternally once started.
  void SetHangAfterStartJobGeneration(bool hang_after_start) {
    hang_after_start_ = hang_after_start;
  }

  void SetDelayedStartJobGeneration(bool delay_job_start) {
    delay_start_ = delay_job_start;
  }

  void SetDelayedCompleteJobGeneration(bool delay_job_complete) {
    delay_complete_ = delay_job_complete;
  }

  void SetMustNotReadJobGeneration(bool must_not_read) {
    must_not_read_ = must_not_read;
  }

  net::URLRequestJob* MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  net::URLRequestJob* MaybeInterceptRedirect(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate,
      const GURL& location) const override;

  net::URLRequestJob* MaybeInterceptResponse(
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const override;

  bool IsHandledProtocol(const std::string& scheme) const override {
    return supported_schemes_.count(scheme) > 0;
  }

  bool IsSafeRedirectTarget(const GURL& location) const override {
    return false;
  }

 private:
  ResourceDispatcherHostTest* test_fixture_;
  bool hang_after_start_;
  bool delay_start_;
  bool delay_complete_;
  bool must_not_read_;
  mutable int url_request_jobs_created_count_;
  std::set<std::string> supported_schemes_;

  DISALLOW_COPY_AND_ASSIGN(TestURLRequestJobFactory);
};

// Associated with an URLRequest to determine if the URLRequest gets deleted.
class TestUserData : public base::SupportsUserData::Data {
 public:
  explicit TestUserData(bool* was_deleted)
      : was_deleted_(was_deleted) {
  }

  ~TestUserData() override { *was_deleted_ = true; }

 private:
  bool* was_deleted_;
};

enum GenericResourceThrottleFlags {
  NONE                       = 0,
  DEFER_STARTING_REQUEST     = 1 << 0,
  DEFER_PROCESSING_RESPONSE  = 1 << 1,
  CANCEL_BEFORE_START        = 1 << 2,
  CANCEL_PROCESSING_RESPONSE = 1 << 3,
  MUST_NOT_CACHE_BODY        = 1 << 4,
};

// Throttle that tracks the current throttle blocking a request.  Only one
// can throttle any request at a time.
class GenericResourceThrottle : public ResourceThrottle {
 public:
  // The value is used to indicate that the throttle should not provide
  // a error code when cancelling a request. net::OK is used, because this
  // is not an error code.
  static const int USE_DEFAULT_CANCEL_ERROR_CODE = net::OK;

  GenericResourceThrottle(int flags, int code)
      : flags_(flags),
        error_code_for_cancellation_(code) {
  }

  ~GenericResourceThrottle() override {
    if (active_throttle_ == this)
      active_throttle_ = nullptr;
  }

  // ResourceThrottle implementation:
  void WillStartRequest(bool* defer) override {
    ASSERT_EQ(nullptr, active_throttle_);
    if (flags_ & DEFER_STARTING_REQUEST) {
      active_throttle_ = this;
      *defer = true;
    }

    if (flags_ & CANCEL_BEFORE_START) {
      if (error_code_for_cancellation_ == USE_DEFAULT_CANCEL_ERROR_CODE) {
        Cancel();
      } else {
        CancelWithError(error_code_for_cancellation_);
      }
    }
  }

  void WillProcessResponse(bool* defer) override {
    ASSERT_EQ(nullptr, active_throttle_);
    if (flags_ & DEFER_PROCESSING_RESPONSE) {
      active_throttle_ = this;
      *defer = true;
    }

    if (flags_ & CANCEL_PROCESSING_RESPONSE) {
      if (error_code_for_cancellation_ == USE_DEFAULT_CANCEL_ERROR_CODE) {
        Cancel();
      } else {
        CancelWithError(error_code_for_cancellation_);
      }
    }
  }

  const char* GetNameForLogging() const override {
    return "GenericResourceThrottle";
  }

  void AssertAndResume() {
    ASSERT_TRUE(this == active_throttle_);
    active_throttle_ = nullptr;
    ResourceThrottle::Resume();
  }

  static GenericResourceThrottle* active_throttle() {
    return active_throttle_;
  }

  bool MustProcessResponseBeforeReadingBody() override {
    return flags_ & MUST_NOT_CACHE_BODY;
  }

 private:
  int flags_;  // bit-wise union of GenericResourceThrottleFlags.
  int error_code_for_cancellation_;

  // The currently active throttle, if any.
  static GenericResourceThrottle* active_throttle_;
};
// static
GenericResourceThrottle* GenericResourceThrottle::active_throttle_ = nullptr;

class TestResourceDispatcherHostDelegate
    : public ResourceDispatcherHostDelegate {
 public:
  TestResourceDispatcherHostDelegate()
      : create_two_throttles_(false),
        flags_(NONE),
        error_code_for_cancellation_(
            GenericResourceThrottle::USE_DEFAULT_CANCEL_ERROR_CODE) {
  }

  void set_url_request_user_data(base::SupportsUserData::Data* user_data) {
    user_data_.reset(user_data);
  }

  void set_flags(int value) {
    flags_ = value;
  }

  void set_error_code_for_cancellation(int code) {
    error_code_for_cancellation_ = code;
  }

  void set_create_two_throttles(bool create_two_throttles) {
    create_two_throttles_ = create_two_throttles;
  }

  // ResourceDispatcherHostDelegate implementation:

  void RequestBeginning(
      net::URLRequest* request,
      ResourceContext* resource_context,
      AppCacheService* appcache_service,
      ResourceType resource_type,
      std::vector<std::unique_ptr<ResourceThrottle>>* throttles) override {
    if (user_data_) {
      const void* key = user_data_.get();
      request->SetUserData(key, std::move(user_data_));
    }

    if (flags_ != NONE) {
      throttles->push_back(std::make_unique<GenericResourceThrottle>(
          flags_, error_code_for_cancellation_));
      if (create_two_throttles_)
        throttles->push_back(std::make_unique<GenericResourceThrottle>(
            flags_, error_code_for_cancellation_));
    }
  }

 private:
  bool create_two_throttles_;
  int flags_;
  int error_code_for_cancellation_;
  std::unique_ptr<base::SupportsUserData::Data> user_data_;
};

// Waits for a ShareableFileReference to be released.
class ShareableFileReleaseWaiter {
 public:
  explicit ShareableFileReleaseWaiter(const base::FilePath& path) {
    scoped_refptr<ShareableFileReference> file =
        ShareableFileReference::Get(path);
    file->AddFinalReleaseCallback(base::BindOnce(
        &ShareableFileReleaseWaiter::Released, base::Unretained(this)));
  }

  void Wait() {
    loop_.Run();
  }

 private:
  void Released(const base::FilePath& path) {
    loop_.Quit();
  }

  base::RunLoop loop_;

  DISALLOW_COPY_AND_ASSIGN(ShareableFileReleaseWaiter);
};

enum class TestMode {
  kWithoutOutOfBlinkCors,
  kWithOutOfBlinkCors,
};

class ResourceDispatcherHostTest : public testing::TestWithParam<TestMode> {
 public:
  typedef ResourceDispatcherHostImpl::LoadInfo LoadInfo;
  typedef ResourceDispatcherHostImpl::LoadInfoList LoadInfoList;
  typedef ResourceDispatcherHostImpl::LoadInfoMap LoadInfoMap;

  ResourceDispatcherHostTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        host_(base::Bind(&DownloadResourceHandler::Create),
              base::ThreadTaskRunnerHandle::Get(),
              /* enable_resource_scheduler */ true),
        use_test_ssl_certificate_(false),
        send_data_received_acks_(false),
        auto_advance_(false) {
    switch (GetParam()) {
      case TestMode::kWithoutOutOfBlinkCors:
        scoped_feature_list_.InitWithFeatures(
            // Enabled features
            {},
            // Disabled features
            {network::features::kOutOfBlinkCORS});
        break;
      case TestMode::kWithOutOfBlinkCors:
        scoped_feature_list_.InitWithFeatures(
            // Enabled features
            {network::features::kOutOfBlinkCORS,
             blink::features::kServiceWorkerServicification},
            // Disabled features
            {});
        break;
    }
    host_.SetLoaderDelegate(&loader_delegate_);
    browser_context_.reset(new TestBrowserContext());
    BrowserContext::EnsureResourceContextInitialized(browser_context_.get());
    content::RunAllTasksUntilIdle();

    filter_ = MakeTestFilter();
    // TODO(cbentzel): Better way to get URLRequestContext?
    net::URLRequestContext* request_context =
        browser_context_->GetResourceContext()->GetRequestContext();
    job_factory_.reset(new TestURLRequestJobFactory(this));
    request_context->set_job_factory(job_factory_.get());
    request_context->set_network_delegate(&network_delegate_);
  }

  ~ResourceDispatcherHostTest() override {
    filter_->OnChannelClosing();
    filter_ = nullptr;
    web_contents_filter_ = nullptr;
  }

 protected:
  friend class TestURLRequestJobFactory;

  // testing::Test
  void SetUp() override {
    ChildProcessSecurityPolicyImpl::GetInstance()->Add(0);
    HandleScheme("test");
    scoped_refptr<SiteInstance> site_instance =
        SiteInstance::Create(browser_context_.get());
    web_contents_ =
        WebContents::Create(WebContents::CreateParams(browser_context_.get()));
    web_contents_filter_ = new TestFilterSpecifyingChild(
        browser_context_.get(),
        web_contents_->GetMainFrame()->GetProcess()->GetID());
    child_ids_.insert(web_contents_->GetMainFrame()->GetProcess()->GetID());
    request_context_getter_ = new net::TestURLRequestContextGetter(
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::UI}));
  }

  void TearDown() override {
    web_contents_filter_->OnChannelClosing();
    web_contents_.reset();

    EXPECT_TRUE(URLRequestTestDelayedStartJob::DelayedStartQueueEmpty());
    URLRequestTestDelayedStartJob::ClearQueue();

    for (auto it = child_ids_.begin(); it != child_ids_.end(); ++it) {
      host_.CancelRequestsForProcess(*it);
    }

    host_.Shutdown();

    ChildProcessSecurityPolicyImpl::GetInstance()->Remove(0);

    // Flush the message loop to make application verifiers happy.
    if (ResourceDispatcherHostImpl::Get())
      ResourceDispatcherHostImpl::Get()->CancelRequestsForContext(
          browser_context_->GetResourceContext());

    browser_context_.reset();
    content::RunAllTasksUntilIdle();
  }

  // Creates a new TestFilter and registers it with |child_ids_| so as not
  // to leak per-child state on test shutdown.
  scoped_refptr<TestFilter> MakeTestFilter() {
    auto filter = base::MakeRefCounted<TestFilter>(browser_context_.get());
    child_ids_.insert(filter->child_id());
    return filter;
  }

  // Creates a request using the current test object as the filter and
  // SubResource as the resource type.
  void MakeTestRequest(int render_view_id,
                       int request_id,
                       const GURL& url,
                       network::mojom::URLLoaderRequest loader_request,
                       network::mojom::URLLoaderClientPtr client);
  void MakeTestRequestWithRenderFrame(
      int render_view_id,
      int render_frame_id,
      int request_id,
      const GURL& url,
      ResourceType type,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client);
  // Generates a request using the given filter and resource type.
  void MakeTestRequestWithResourceType(
      ResourceMessageFilter* filter,
      int render_view_id,
      int request_id,
      const GURL& url,
      ResourceType type,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client);

  void MakeWebContentsAssociatedTestRequestWithResourceType(
      int request_id,
      const GURL& url,
      ResourceType type,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client);

  // Generates a request with the given priority.
  void MakeTestRequestWithPriorityAndRenderFrame(
      int render_view_id,
      int render_frame_id,
      int request_id,
      net::RequestPriority priority,
      network::mojom::URLLoaderRequest loader_request,
      network::mojom::URLLoaderClientPtr client);

  void CancelRequest(int request_id);
  void CompleteStartRequest(int request_id);
  void CompleteStartRequest(ResourceMessageFilter* filter, int request_id);

  net::TestNetworkDelegate* network_delegate() { return &network_delegate_; }

  void EnsureSchemeIsAllowed(const std::string& scheme) {
    ChildProcessSecurityPolicyImpl* policy =
        ChildProcessSecurityPolicyImpl::GetInstance();
    if (!policy->IsWebSafeScheme(scheme))
      policy->RegisterWebSafeScheme(scheme);
  }

  // Sets a particular response for any request from now on. To switch back to
  // the default bahavior, pass an empty |headers|. |headers| should be CR[LF]
  // terminated.
  void SetResponse(const std::string& headers, const std::string& data) {
    response_headers_ = headers;
    response_data_ = data;
  }
  void SetResponse(const std::string& headers) {
    SetResponse(headers, std::string());
  }

  // If called, requests called from now on will be created as
  // TestHTTPSURLRequestJobs: that is, a test certificate will be set on
  // the |ssl_info| field of the response.
  void SetTestSSLCertificate() { use_test_ssl_certificate_ = true; }

  // Intercepts requests for the given protocol.
  void HandleScheme(const std::string& scheme) {
    job_factory_->HandleScheme(scheme);
    EnsureSchemeIsAllowed(scheme);
  }

  void DeleteRenderFrame(const GlobalFrameRoutingId& global_routing_id) {
    host_.OnRenderFrameDeleted(global_routing_id);
  }

  // Creates and drives a main resource request until completion. Then asserts
  // that the expected_error_code has been emitted for the request.
  void CompleteFailingMainResourceRequest(const GURL& url,
                                          int expected_error_code) {
    auto_advance_ = true;

    // Make a navigation request.
    TestNavigationURLLoaderDelegate delegate;
    mojom::BeginNavigationParamsPtr begin_params =
        mojom::BeginNavigationParams::New(
            std::string() /* headers */, net::LOAD_NORMAL,
            false /* skip_service_worker */,
            blink::mojom::RequestContextType::LOCATION,
            blink::WebMixedContentContextType::kBlockable,
            false /* is_form_submission */, GURL() /* searchable_form_url */,
            std::string() /* searchable_form_encoding */,
            url::Origin::Create(url), GURL() /* client_side_redirect_url */,
            base::nullopt /* devtools_initiator_info */);
    CommonNavigationParams common_params;
    common_params.url = url;
    std::unique_ptr<NavigationRequestInfo> request_info(
        new NavigationRequestInfo(common_params, std::move(begin_params), url,
                                  true, false, false, -1, false, false, false,
                                  false, nullptr,
                                  base::UnguessableToken::Create(),
                                  base::UnguessableToken::Create()));
    std::unique_ptr<NavigationURLLoader> test_loader =
        NavigationURLLoader::Create(
            browser_context_->GetResourceContext(),
            BrowserContext::GetDefaultStoragePartition(browser_context_.get()),
            std::move(request_info), nullptr, nullptr, nullptr, &delegate);

    // The navigation should fail with the expected error code.
    delegate.WaitForRequestFailed();
    ASSERT_EQ(expected_error_code, delegate.net_error());
  }

  bool IsDetached(net::URLRequest* request) {
    auto* request_info = ResourceRequestInfoImpl::ForRequest(request);
    if (!request_info)
      return false;
    return request_info->detachable_handler() &&
           request_info->detachable_handler()->is_detached();
  }

  bool IsAborted(const network::TestURLLoaderClient& client) {
    // TODO(toyoshim): Once NetworkService or OutOfBlinkCORS is enabled, these
    // expectations below should be receiving a completion with ERR_ABORTED.
    if (!client.has_received_completion())
      return client.has_received_connection_error();

    return client.completion_status().error_code == net::ERR_ABORTED;
  }

  void SetMaxDelayableRequests(size_t max_delayable_requests) {
    network::ResourceSchedulerParamsManager::ParamsForNetworkQualityContainer c;
    for (int i = 0; i != net::EFFECTIVE_CONNECTION_TYPE_LAST; ++i) {
      auto type = static_cast<net::EffectiveConnectionType>(i);
      c[type] =
          network::ResourceSchedulerParamsManager::ParamsForNetworkQuality(
              max_delayable_requests, 0.0, false, base::nullopt);
    }
    host_.scheduler()->SetResourceSchedulerParamsManagerForTests(
        network::ResourceSchedulerParamsManager(c));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  std::unique_ptr<TestBrowserContext> browser_context_;
  std::unique_ptr<TestURLRequestJobFactory> job_factory_;
  std::unique_ptr<WebContents> web_contents_;
  scoped_refptr<TestFilter> filter_;

  scoped_refptr<TestFilterSpecifyingChild> web_contents_filter_;
  net::TestNetworkDelegate network_delegate_;
  LoaderDelegateImpl loader_delegate_;
  ResourceDispatcherHostImpl host_;
  std::string response_headers_;
  std::string response_data_;
  bool use_test_ssl_certificate_;
  std::string scheme_;
  bool send_data_received_acks_;
  std::set<int> child_ids_;
  std::unique_ptr<base::RunLoop> wait_for_request_complete_loop_;
  RenderViewHostTestEnabler render_view_host_test_enabler_;
  bool auto_advance_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_getter_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

void ResourceDispatcherHostTest::MakeTestRequest(
    int render_view_id,
    int request_id,
    const GURL& url,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client) {
  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  url, RESOURCE_TYPE_SUB_RESOURCE,
                                  std::move(loader_request), std::move(client));
}

void ResourceDispatcherHostTest::MakeTestRequestWithRenderFrame(
    int render_view_id,
    int render_frame_id,
    int request_id,
    const GURL& url,
    ResourceType type,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client) {
  network::ResourceRequest request = CreateResourceRequest("GET", type, url);
  request.render_frame_id = render_frame_id;
  filter_->CreateLoaderAndStart(
      std::move(loader_request), render_view_id, request_id,
      network::mojom::kURLLoadOptionSniffMimeType, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
}

void ResourceDispatcherHostTest::MakeTestRequestWithResourceType(
    ResourceMessageFilter* filter,
    int render_view_id,
    int request_id,
    const GURL& url,
    ResourceType type,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client) {
  network::ResourceRequest request = CreateResourceRequest("GET", type, url);
  filter->CreateLoaderAndStart(
      std::move(loader_request), render_view_id, request_id,
      network::mojom::kURLLoadOptionSniffMimeType, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
}

void ResourceDispatcherHostTest::
    MakeWebContentsAssociatedTestRequestWithResourceType(
        int request_id,
        const GURL& url,
        ResourceType type,
        network::mojom::URLLoaderRequest loader_request,
        network::mojom::URLLoaderClientPtr client) {
  network::ResourceRequest request = CreateResourceRequest("GET", type, url);
  DCHECK_EQ(web_contents_filter_->child_id(),
            web_contents_->GetMainFrame()->GetProcess()->GetID());
  request.render_frame_id = web_contents_->GetMainFrame()->GetRoutingID();
  web_contents_filter_->CreateLoaderAndStart(
      std::move(loader_request), 0, request_id,
      network::mojom::kURLLoadOptionNone, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
}

void ResourceDispatcherHostTest::MakeTestRequestWithPriorityAndRenderFrame(
    int render_view_id,
    int render_frame_id,
    int request_id,
    net::RequestPriority priority,
    network::mojom::URLLoaderRequest loader_request,
    network::mojom::URLLoaderClientPtr client) {
  network::ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_SUB_RESOURCE, GURL("http://example.com/priority"));
  request.render_frame_id = render_frame_id;
  request.priority = priority;
  filter_->CreateLoaderAndStart(
      std::move(loader_request), render_view_id, request_id,
      network::mojom::kURLLoadOptionNone, request, std::move(client),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
}

void ResourceDispatcherHostTest::CancelRequest(int request_id) {
  host_.CancelRequest(filter_->child_id(), request_id);
}

void ResourceDispatcherHostTest::CompleteStartRequest(int request_id) {
  CompleteStartRequest(filter_.get(), request_id);
}

void ResourceDispatcherHostTest::CompleteStartRequest(
    ResourceMessageFilter* filter,
    int request_id) {
  GlobalRequestID gid(filter->child_id(), request_id);
  net::URLRequest* req = host_.GetURLRequest(gid);
  EXPECT_TRUE(req);
  if (req)
    URLRequestTestDelayedStartJob::CompleteStart(req);
}

void CheckSuccessfulRequest(network::TestURLLoaderClient* client,
                            const std::string& reference_data) {
  if (!reference_data.empty()) {
    client->RunUntilResponseBodyArrived();
    mojo::ScopedDataPipeConsumerHandle body = client->response_body_release();
    ASSERT_TRUE(body.is_valid());

    std::string actual;
    EXPECT_TRUE(mojo::BlockingCopyToString(std::move(body), &actual));
    EXPECT_EQ(reference_data, actual);
  }
  client->RunUntilComplete();
  EXPECT_FALSE(client->response_body().is_valid());
  EXPECT_EQ(net::OK, client->completion_status().error_code);
}

// Tests whether messages get canceled properly. We issue four requests,
// cancel two of them, and make sure that each sent the proper notifications.
TEST_P(ResourceDispatcherHostTest, Cancel) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4;
  network::TestURLLoaderClient client1, client2, client3, client4;
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1(),
                  mojo::MakeRequest(&loader1), client1.CreateInterfacePtr());
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2(),
                  mojo::MakeRequest(&loader2), client2.CreateInterfacePtr());
  MakeTestRequest(0, 3, net::URLRequestTestJob::test_url_3(),
                  mojo::MakeRequest(&loader3), client3.CreateInterfacePtr());

  MakeTestRequestWithResourceType(
      filter_.get(), 0, 4, net::URLRequestTestJob::test_url_4(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader4), client4.CreateInterfacePtr());

  CancelRequest(2);

  // Cancel request must come from the renderer for a detachable resource to
  // delay.
  loader4 = nullptr;
  content::RunAllTasksUntilIdle();

  // The handler should have been detached now.
  GlobalRequestID global_request_id(filter_->child_id(), 4);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler()->is_detached());

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // Everything should be out now.
  EXPECT_EQ(0, host_.pending_requests());

  // Check that request 2 and 4 got canceled, as far as the renderer is
  // concerned.  Request 2 will have been deleted.
  client2.RunUntilConnectionError();
  client4.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client4.completion_status().error_code);

  // However, request 4 should have actually gone to completion. (Only request 2
  // was canceled.)
  EXPECT_EQ(4, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// Shows that detachable requests will timeout if the request takes too long to
// complete.
TEST_P(ResourceDispatcherHostTest, DetachedResourceTimesOut) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  MakeTestRequestWithResourceType(
      filter_.get(), 0, 1, net::URLRequestTestJob::test_url_2(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();

  GlobalRequestID global_request_id(filter_->child_id(), 1);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler());
  info->detachable_handler()->set_cancel_delay(
      base::TimeDelta::FromMilliseconds(200));

  // Cancel the request handled by |loader|.
  loader = nullptr;

  // From the renderer's perspective, the request was cancelled.
  content::RunAllTasksUntilIdle();
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);

  // But it continues detached.
  EXPECT_EQ(1, host_.pending_requests());
  EXPECT_TRUE(info->detachable_handler()->is_detached());

  // Wait until after the delay timer times out before we start processing any
  // messages.
  base::OneShotTimer timer;
  base::RunLoop run_loop;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(210),
              run_loop.QuitWhenIdleClosure());
  run_loop.Run();

  // The prefetch should be cancelled by now.
  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// If the filter has disappeared then detachable resources should continue to
// load.
TEST_P(ResourceDispatcherHostTest, DeletedFilterDetached) {
  network::mojom::URLLoaderPtr loader1;
  network::TestURLLoaderClient client1;
  base::test::ScopedFeatureList feature_list;
  // test_url_1's data is available synchronously, so use 2 and 3.
  network::ResourceRequest request_prefetch = CreateResourceRequest(
      "GET", RESOURCE_TYPE_PREFETCH, net::URLRequestTestJob::test_url_2());

  filter_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader1), 0, 1, network::mojom::kURLLoadOptionNone,
      request_prefetch, client1.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Remove the filter before processing the requests by simulating channel
  // closure.
  ResourceRequestInfoImpl* info_prefetch = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  DCHECK_EQ(filter_.get(), info_prefetch->requester_info()->filter());
  filter_->OnChannelClosing();

  content::RunAllTasksUntilIdle();
  DCHECK(IsAborted(client1));

  // But it continues detached.
  EXPECT_EQ(1, host_.pending_requests());
  EXPECT_TRUE(info_prefetch->detachable_handler()->is_detached());

  // Make sure the requests weren't canceled early.
  EXPECT_EQ(1, host_.pending_requests());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// If the filter has disappeared (original process dies) then detachable
// resources should continue to load, even when redirected.
TEST_P(ResourceDispatcherHostTest, DeletedFilterDetachedRedirect) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  network::ResourceRequest request = CreateResourceRequest(
      "GET", RESOURCE_TYPE_PREFETCH,
      net::URLRequestTestJob::test_url_redirect_to_url_2());

  filter_->CreateLoaderAndStart(
      mojo::MakeRequest(&loader), 0, 1, network::mojom::kURLLoadOptionNone,
      request, client.CreateInterfacePtr(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Remove the filter before processing the request by simulating channel
  // closure.
  GlobalRequestID global_request_id(filter_->child_id(), 1);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  info->requester_info()->filter()->OnChannelClosing();

  // The request should be detached.
  EXPECT_EQ(1, host_.pending_requests());
  EXPECT_TRUE(info->detachable_handler()->is_detached());

  // Verify no redirects before resetting the filter.
  net::URLRequest* url_request = host_.GetURLRequest(global_request_id);
  EXPECT_EQ(1u, url_request->url_chain().size());

  // From the renderer's perspective, the request was cancelled.
  content::RunAllTasksUntilIdle();
  DCHECK(IsAborted(client));

  content::RunAllTasksUntilIdle();
  // Verify that a redirect was followed.
  EXPECT_EQ(2u, url_request->url_chain().size());

  // Make sure the request wasn't canceled early.
  EXPECT_EQ(1, host_.pending_requests());

  // Finish up the request.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0, host_.pending_requests());
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

TEST_P(ResourceDispatcherHostTest, CancelWhileStartIsDeferred) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  bool was_deleted = false;

  // Arrange to have requests deferred before starting.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_STARTING_REQUEST);
  delegate.set_url_request_user_data(new TestUserData(&was_deleted));
  host_.SetDelegate(&delegate);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1(),
                  mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();
  // We cancel from the renderer because all non-renderer cancels delete
  // the request synchronously.
  host_.CancelRequestFromRenderer(GlobalRequestID(filter_->child_id(), 1));

  // Our TestResourceThrottle should not have been deleted yet.  This is to
  // ensure that destruction of the URLRequest happens asynchronously to
  // calling CancelRequest.
  EXPECT_FALSE(was_deleted);

  content::RunAllTasksUntilIdle();

  EXPECT_TRUE(was_deleted);
}

TEST_P(ResourceDispatcherHostTest, DetachWhileStartIsDeferred) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  bool was_deleted = false;

  // Arrange to have requests deferred before starting.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_STARTING_REQUEST);
  delegate.set_url_request_user_data(new TestUserData(&was_deleted));
  host_.SetDelegate(&delegate);

  MakeTestRequestWithResourceType(
      filter_.get(), 0, 1, net::URLRequestTestJob::test_url_1(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  // Cancel request must come from the renderer for a detachable resource to
  // detach.
  loader = nullptr;

  // Even after driving the event loop, the request has not been deleted.
  EXPECT_FALSE(was_deleted);

  // However, it is still throttled because the defer happened above the
  // DetachableResourceHandler.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();
  EXPECT_FALSE(was_deleted);

  // Resume the request.
  GenericResourceThrottle* throttle =
      GenericResourceThrottle::active_throttle();
  ASSERT_TRUE(throttle);
  throttle->AssertAndResume();

  // Now, the request completes.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();
  EXPECT_TRUE(was_deleted);
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// Tests if cancel is called in ResourceThrottle::WillStartRequest, then the
// URLRequest will not be started.
TEST_P(ResourceDispatcherHostTest, CancelInResourceThrottleWillStartRequest) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(CANCEL_BEFORE_START);
  host_.SetDelegate(&delegate);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1(),
                  mojo::MakeRequest(&loader), client.CreateInterfacePtr());

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // Check that request got canceled.
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);

  // Make sure URLRequest is never started.
  EXPECT_EQ(0, job_factory_->url_request_jobs_created_count());
}

TEST_P(ResourceDispatcherHostTest, PausedStartError) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  // Arrange to have requests deferred before processing response headers.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_PROCESSING_RESPONSE);
  host_.SetDelegate(&delegate);

  job_factory_->SetDelayedStartJobGeneration(true);
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_error(),
                  mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  CompleteStartRequest(1);

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(0, host_.pending_requests());
}

TEST_P(ResourceDispatcherHostTest, ThrottleAndResumeTwice) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  // Arrange to have requests deferred before starting.
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(DEFER_STARTING_REQUEST);
  delegate.set_create_two_throttles(true);
  host_.SetDelegate(&delegate);

  // Make sure the first throttle blocked the request, and then resume.
  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1(),
                  mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  GenericResourceThrottle* first_throttle =
      GenericResourceThrottle::active_throttle();
  ASSERT_TRUE(first_throttle);
  first_throttle->AssertAndResume();

  // Make sure the second throttle blocked the request, and then resume.
  ASSERT_TRUE(GenericResourceThrottle::active_throttle());
  ASSERT_NE(first_throttle, GenericResourceThrottle::active_throttle());
  GenericResourceThrottle::active_throttle()->AssertAndResume();

  ASSERT_FALSE(GenericResourceThrottle::active_throttle());

  // The request is started asynchronously.
  content::RunAllTasksUntilIdle();

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.pending_requests());

  // Make sure the request completed successfully.
  CheckSuccessfulRequest(&client, net::URLRequestTestJob::test_data_1());
}

// Tests that the delegate can cancel a request and provide a error code.
TEST_P(ResourceDispatcherHostTest, CancelInDelegate) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  TestResourceDispatcherHostDelegate delegate;
  delegate.set_flags(CANCEL_BEFORE_START);
  delegate.set_error_code_for_cancellation(net::ERR_ACCESS_DENIED);
  host_.SetDelegate(&delegate);

  MakeTestRequest(0, 1, net::URLRequestTestJob::test_url_1(),
                  mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  // The request will get cancelled by the throttle.

  // flush all the pending requests
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ACCESS_DENIED, client.completion_status().error_code);
}

TEST_P(ResourceDispatcherHostTest, CancelRequestsForRoute) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4;
  network::TestURLLoaderClient client1, client2, client3, client4;
  base::test::ScopedFeatureList feature_list;
  job_factory_->SetDelayedStartJobGeneration(true);
  MakeTestRequestWithRenderFrame(0, 11, 1, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_XHR, mojo::MakeRequest(&loader1),
                                 client1.CreateInterfacePtr());
  EXPECT_EQ(1, host_.pending_requests());

  MakeTestRequestWithRenderFrame(0, 12, 2, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_XHR, mojo::MakeRequest(&loader2),
                                 client2.CreateInterfacePtr());
  EXPECT_EQ(2, host_.pending_requests());

  MakeTestRequestWithRenderFrame(
      0, 11, 3, net::URLRequestTestJob::test_url_3(), RESOURCE_TYPE_PREFETCH,
      mojo::MakeRequest(&loader3), client3.CreateInterfacePtr());
  EXPECT_EQ(3, host_.pending_requests());

  MakeTestRequestWithRenderFrame(
      0, 12, 4, net::URLRequestTestJob::test_url_4(), RESOURCE_TYPE_PREFETCH,
      mojo::MakeRequest(&loader4), client4.CreateInterfacePtr());
  EXPECT_EQ(4, host_.pending_requests());

  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 2)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 3)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 4)));

  host_.CancelRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 11));

  EXPECT_FALSE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 1)));
  EXPECT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 2)));
  ASSERT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 3)));
  ASSERT_TRUE(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 4)));

  EXPECT_TRUE(
      IsDetached(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 3))));
  EXPECT_FALSE(
      IsDetached(host_.GetURLRequest(GlobalRequestID(filter_->child_id(), 4))));

  CompleteStartRequest(2);
  CompleteStartRequest(3);
  CompleteStartRequest(4);

  while (host_.pending_requests() > 0) {
    while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
    }
    content::RunAllTasksUntilIdle();
  }
}

// Tests CancelRequestsForProcess
TEST_P(ResourceDispatcherHostTest, TestProcessCancel) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4;
  network::TestURLLoaderClient client1, client2, client3, client4;
  scoped_refptr<TestFilter> test_filter = MakeTestFilter();

  // request 1 goes to the test delegate
  MakeTestRequestWithResourceType(
      test_filter.get(), 0, 1, net::URLRequestTestJob::test_url_1(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader1),
      client1.CreateInterfacePtr());

  // request 2 goes to us
  MakeTestRequest(0, 2, net::URLRequestTestJob::test_url_2(),
                  mojo::MakeRequest(&loader2), client2.CreateInterfacePtr());

  // request 3 goes to the test delegate
  MakeTestRequestWithResourceType(
      test_filter.get(), 0, 3, net::URLRequestTestJob::test_url_3(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader3),
      client3.CreateInterfacePtr());

  // request 4 goes to us
  MakeTestRequestWithResourceType(
      filter_.get(), 0, 4, net::URLRequestTestJob::test_url_4(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader4), client4.CreateInterfacePtr());

  // Make sure all requests have finished stage one. test_url_1 will have
  // finished.
  content::RunAllTasksUntilIdle();

  // TODO(mbelshe):
  // Now that the async IO path is in place, the IO always completes on the
  // initial call; so the requests have already completed.  This basically
  // breaks the whole test.
  // EXPECT_EQ(3, host_.pending_requests());

  // Process test_url_2 and test_url_3 for one level so one callback is called.
  // We'll cancel test_url_4 (detachable) before processing it to verify that it
  // delays the cancel.
  for (int i = 0; i < 2; i++)
    EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());

  // Cancel the requests to the test process.
  host_.CancelRequestsForProcess(filter_->child_id());

  // The requests should all be cancelled, except request 4, which is detached.
  EXPECT_EQ(1, host_.pending_requests());
  GlobalRequestID global_request_id(filter_->child_id(), 4);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler()->is_detached());

  // Flush all the pending requests.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_EQ(0, host_.pending_requests());

  client1.RunUntilConnectionError();
  CheckSuccessfulRequest(&client2, net::URLRequestTestJob::test_data_2());
  client3.RunUntilConnectionError();
  client4.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client4.completion_status().error_code);

  // For the network stack, no requests were canceled.
  EXPECT_EQ(4, network_delegate()->completed_requests());
  EXPECT_EQ(0, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());

  test_filter->OnChannelClosing();
}

// Tests whether the correct requests get canceled when a RenderViewHost is
// deleted.
TEST_P(ResourceDispatcherHostTest, CancelRequestsOnRenderFrameDeleted) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4, loader5;
  network::TestURLLoaderClient client1, client2, client3, client4, client5;
  // Requests all hang once started.  This prevents requests from being
  // destroyed due to completion.
  job_factory_->SetHangAfterStartJobGeneration(true);
  HandleScheme("http");

  TestResourceDispatcherHostDelegate delegate;
  host_.SetDelegate(&delegate);
  host_.OnRenderViewHostCreated(filter_->child_id(), 0,
                                request_context_getter_.get());
  SetMaxDelayableRequests(1);

  // One RenderView issues a high priority request and a low priority one. Both
  // should be started.
  MakeTestRequestWithPriorityAndRenderFrame(0, 10, 1, net::HIGHEST,
                                            mojo::MakeRequest(&loader1),
                                            client1.CreateInterfacePtr());
  MakeTestRequestWithPriorityAndRenderFrame(0, 11, 2, net::LOWEST,
                                            mojo::MakeRequest(&loader2),
                                            client2.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, network_delegate_.created_requests());
  EXPECT_EQ(0, network_delegate_.canceled_requests());

  // The same RenderView issues two more low priority requests. The
  // ResourceScheduler shouldn't let them start immediately.
  MakeTestRequestWithPriorityAndRenderFrame(0, 10, 3, net::LOWEST,
                                            mojo::MakeRequest(&loader3),
                                            client3.CreateInterfacePtr());
  MakeTestRequestWithPriorityAndRenderFrame(0, 11, 4, net::LOWEST,
                                            mojo::MakeRequest(&loader4),
                                            client4.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(2, network_delegate_.created_requests());
  EXPECT_EQ(0, network_delegate_.canceled_requests());

  // Another RenderView in the same process as the old one issues a request,
  // which is then started.
  MakeTestRequestWithPriorityAndRenderFrame(1, 12, 5, net::LOWEST,
                                            mojo::MakeRequest(&loader5),
                                            client5.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, network_delegate_.created_requests());
  EXPECT_EQ(0, network_delegate_.canceled_requests());

  // The first two RenderFrameHosts are destroyed.  All 4 of their requests
  // should be cancelled, and none of the two deferred requests should be
  // started.
  DeleteRenderFrame(GlobalFrameRoutingId(filter_->child_id(), 10));
  DeleteRenderFrame(GlobalFrameRoutingId(filter_->child_id(), 11));
  host_.OnRenderViewHostDeleted(filter_->child_id(), 0);
  content::RunAllTasksUntilIdle();
  EXPECT_EQ(3, network_delegate_.created_requests());
  EXPECT_EQ(4, network_delegate_.canceled_requests());
}

TEST_P(ResourceDispatcherHostTest, TestProcessCancelDetachedTimesOut) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  MakeTestRequestWithResourceType(
      filter_.get(), 0, 1, net::URLRequestTestJob::test_url_4(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();
  GlobalRequestID global_request_id(filter_->child_id(), 1);
  ResourceRequestInfoImpl* info = ResourceRequestInfoImpl::ForRequest(
      host_.GetURLRequest(global_request_id));
  ASSERT_TRUE(info->detachable_handler());
  info->detachable_handler()->set_cancel_delay(
      base::TimeDelta::FromMilliseconds(200));
  content::RunAllTasksUntilIdle();

  // Cancel the requests to the test process.
  host_.CancelRequestsForProcess(filter_->child_id());
  EXPECT_EQ(1, host_.pending_requests());

  // Wait until after the delay timer times out before we start processing any
  // messages.
  base::RunLoop run_loop;
  base::OneShotTimer timer;
  timer.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(210),
              base::BindOnce(&base::RunLoop::QuitWhenIdle,
                             base::Unretained(&run_loop)));
  run_loop.Run();

  // The prefetch should be cancelled by now.
  EXPECT_EQ(0, host_.pending_requests());

  // In case any messages are still to be processed.
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  content::RunAllTasksUntilIdle();

  // The request should have cancelled.
  client.RunUntilComplete();
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
  // And not run to completion.
  EXPECT_EQ(1, network_delegate()->completed_requests());
  EXPECT_EQ(1, network_delegate()->canceled_requests());
  EXPECT_EQ(0, network_delegate()->error_count());
}

// Tests blocking and resuming requests.
TEST_P(ResourceDispatcherHostTest, TestBlockingResumingRequests) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4, loader5,
      loader6, loader7;
  network::TestURLLoaderClient client1, client2, client3, client4, client5,
      client6, client7;

  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 11));
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 12));
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 13));

  MakeTestRequestWithRenderFrame(0, 10, 1, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader1),
                                 client1.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(1, 11, 2, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader2),
                                 client2.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(0, 10, 3, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader3),
                                 client3.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(1, 11, 4, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader4),
                                 client4.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(2, 12, 5, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader5),
                                 client5.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(3, 13, 6, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader6),
                                 client6.CreateInterfacePtr());

  // Flush all the pending requests
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  CheckSuccessfulRequest(&client1, net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(&client3, net::URLRequestTestJob::test_data_3());
  EXPECT_FALSE(client2.has_received_completion());
  EXPECT_FALSE(client4.has_received_completion());
  EXPECT_FALSE(client5.has_received_completion());
  EXPECT_FALSE(client6.has_received_completion());

  // Resume requests for RFH 11 and flush pending requests.
  host_.ResumeBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 11));
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  CheckSuccessfulRequest(&client2, net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(&client4, net::URLRequestTestJob::test_data_1());
  EXPECT_FALSE(client5.has_received_completion());
  EXPECT_FALSE(client6.has_received_completion());

  // Test that new requests are not blocked for RFH 11.
  MakeTestRequestWithRenderFrame(1, 11, 7, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader7),
                                 client7.CreateInterfacePtr());
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}
  CheckSuccessfulRequest(&client7, net::URLRequestTestJob::test_data_1());
  EXPECT_FALSE(client5.has_received_completion());
  EXPECT_FALSE(client6.has_received_completion());

  // Now resumes requests for all RFH (12 and 13).
  host_.ResumeBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 12));
  host_.ResumeBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 13));
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  CheckSuccessfulRequest(&client5, net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(&client6, net::URLRequestTestJob::test_data_3());
}

// Tests blocking and canceling requests.
TEST_P(ResourceDispatcherHostTest, TestBlockingCancelingRequests) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4, loader5;
  network::TestURLLoaderClient client1, client2, client3, client4, client5;

  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 11));

  MakeTestRequestWithRenderFrame(0, 10, 1, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader1),
                                 client1.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(1, 11, 2, net::URLRequestTestJob::test_url_2(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader2),
                                 client2.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(0, 10, 3, net::URLRequestTestJob::test_url_3(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader3),
                                 client3.CreateInterfacePtr());
  MakeTestRequestWithRenderFrame(1, 11, 4, net::URLRequestTestJob::test_url_1(),
                                 RESOURCE_TYPE_SUB_RESOURCE,
                                 mojo::MakeRequest(&loader4),
                                 client4.CreateInterfacePtr());
  // Blocked detachable resources should not delay cancellation.
  //
  MakeTestRequestWithRenderFrame(1, 11, 5, net::URLRequestTestJob::test_url_4(),
                                 RESOURCE_TYPE_PREFETCH,  // detachable type
                                 mojo::MakeRequest(&loader5),
                                 client5.CreateInterfacePtr());
  // Flush all the pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  CheckSuccessfulRequest(&client1, net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(&client3, net::URLRequestTestJob::test_data_3());
  EXPECT_FALSE(IsAborted(client2));
  EXPECT_FALSE(IsAborted(client4));
  EXPECT_FALSE(IsAborted(client5));

  // Cancel requests for RFH 11.
  host_.CancelBlockedRequestsForRoute(
      GlobalFrameRoutingId(filter_->child_id(), 11));
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  EXPECT_TRUE(IsAborted(client2));
  EXPECT_TRUE(IsAborted(client4));
  EXPECT_TRUE(IsAborted(client5));
}

// Tests that blocked requests are canceled if their associated process dies.
TEST_P(ResourceDispatcherHostTest, TestBlockedRequestsProcessDies) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4, loader5;
  network::TestURLLoaderClient client1, client2, client3, client4, client5;
  // This second filter is used to emulate a second process.
  scoped_refptr<TestFilter> second_filter = MakeTestFilter();

  host_.BlockRequestsForRoute(
      GlobalFrameRoutingId(second_filter->child_id(), 0));

  MakeTestRequestWithResourceType(
      filter_.get(), 0, 1, net::URLRequestTestJob::test_url_1(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader1),
      client1.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, 2, net::URLRequestTestJob::test_url_2(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader2),
      client2.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 0, 3, net::URLRequestTestJob::test_url_3(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader3),
      client3.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, 4, net::URLRequestTestJob::test_url_1(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader4),
      client4.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, 5, net::URLRequestTestJob::test_url_4(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader5), client5.CreateInterfacePtr());

  // Simulate process death.
  host_.CancelRequestsForProcess(second_filter->child_id());

  // Flush all the pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  CheckSuccessfulRequest(&client1, net::URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(&client3, net::URLRequestTestJob::test_data_3());
  EXPECT_TRUE(IsAborted(client2));
  EXPECT_TRUE(IsAborted(client4));
  EXPECT_TRUE(IsAborted(client5));

  EXPECT_TRUE(host_.blocked_loaders_map_.empty());
  second_filter->OnChannelClosing();
}

// Tests that blocked requests don't leak when the ResourceDispatcherHost goes
// away.  Note that we rely on Purify for finding the leaks if any.
// If this test turns the Purify bot red, check the ResourceDispatcherHost
// destructor to make sure the blocked requests are deleted.
TEST_P(ResourceDispatcherHostTest, TestBlockedRequestsDontLeak) {
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4, loader5,
      loader6, loader7, loader8;
  network::TestURLLoaderClient client1, client2, client3, client4, client5,
      client6, client7, client8;
  // This second filter is used to emulate a second process.
  scoped_refptr<TestFilter> second_filter = MakeTestFilter();

  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 1));
  host_.BlockRequestsForRoute(GlobalFrameRoutingId(filter_->child_id(), 2));
  host_.BlockRequestsForRoute(
      GlobalFrameRoutingId(second_filter->child_id(), 1));

  MakeTestRequestWithResourceType(
      filter_.get(), 0, 1, net::URLRequestTestJob::test_url_1(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader1),
      client1.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 1, 2, net::URLRequestTestJob::test_url_2(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader2),
      client2.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 0, 3, net::URLRequestTestJob::test_url_3(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader3),
      client3.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      second_filter.get(), 1, 4, net::URLRequestTestJob::test_url_1(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader4),
      client4.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 2, 5, net::URLRequestTestJob::test_url_2(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader5),
      client5.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 2, 6, net::URLRequestTestJob::test_url_3(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loader6),
      client6.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 0, 7, net::URLRequestTestJob::test_url_4(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader7), client7.CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      second_filter.get(), 1, 8, net::URLRequestTestJob::test_url_4(),
      RESOURCE_TYPE_PREFETCH,  // detachable type
      mojo::MakeRequest(&loader8), client8.CreateInterfacePtr());

  host_.CancelRequestsForProcess(filter_->child_id());
  host_.CancelRequestsForProcess(second_filter->child_id());

  // Flush all the pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  second_filter->OnChannelClosing();
}

// Test the private helper method "CalculateApproximateMemoryCost()".
TEST_P(ResourceDispatcherHostTest, CalculateApproximateMemoryCost) {
  net::URLRequestContext context;
  std::unique_ptr<net::URLRequest> req(context.CreateRequest(
      GURL("http://www.google.com"), net::DEFAULT_PRIORITY, nullptr,
      TRAFFIC_ANNOTATION_FOR_TESTS));
  EXPECT_EQ(4425, ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
                      req.get()));

  // Add 9 bytes of referrer.
  req->SetReferrer("123456789");
  EXPECT_EQ(4434, ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
                      req.get()));

  // Add 33 bytes of upload content.
  std::string upload_content;
  upload_content.resize(33);
  std::fill(upload_content.begin(), upload_content.end(), 'x');
  std::unique_ptr<net::UploadElementReader> reader(
      new net::UploadBytesElementReader(upload_content.data(),
                                        upload_content.size()));
  req->set_upload(
      net::ElementsUploadDataStream::CreateWithReader(std::move(reader), 0));

  // Since the upload throttling is disabled, this has no effect on the cost.
  EXPECT_EQ(4434, ResourceDispatcherHostImpl::CalculateApproximateMemoryCost(
                      req.get()));
}

// Test that too much memory for outstanding requests for a particular
// render_process_host_id causes requests to fail.
TEST_P(ResourceDispatcherHostTest, TooMuchOutstandingRequestsMemory) {
  // Expected cost of each request as measured by
  // ResourceDispatcherHost::CalculateApproximateMemoryCost().
  const int kMemoryCostOfTest2Req =
      ResourceDispatcherHostImpl::kAvgBytesPerOutstandingRequest +
      net::URLRequestTestJob::test_url_2().spec().size() + sizeof("GET") - 1;

  // Tighten the bound on the ResourceDispatcherHost, to speed things up.
  constexpr int kMaxCostPerProcess = 440000;
  host_.set_max_outstanding_requests_cost_per_process(kMaxCostPerProcess);

  // Determine how many instance of test_url_2() we can request before
  // throttling kicks in.
  const size_t kMaxRequests = kMaxCostPerProcess / kMemoryCostOfTest2Req;

  auto loaders =
      std::make_unique<network::mojom::URLLoaderPtr[]>(kMaxRequests + 4);
  auto clients =
      std::make_unique<network::TestURLLoaderClient[]>(kMaxRequests + 4);
  network::mojom::URLLoaderPtr loader1, loader2, loader3, loader4;
  network::TestURLLoaderClient client1, client2, client3, client4;

  // This second filter is used to emulate a second process.
  scoped_refptr<TestFilter> second_filter = MakeTestFilter();

  // Saturate the number of outstanding requests for our process.
  for (size_t i = 0; i < kMaxRequests; ++i) {
    MakeTestRequestWithResourceType(
        filter_.get(), 0, i + 1, net::URLRequestTestJob::test_url_2(),
        RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loaders[i]),
        clients[i].CreateInterfacePtr());
  }

  // Issue two more requests for our process -- these should fail immediately.
  MakeTestRequestWithResourceType(
      filter_.get(), 0, kMaxRequests + 1, net::URLRequestTestJob::test_url_2(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loaders[kMaxRequests]),
      clients[kMaxRequests].CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      filter_.get(), 0, kMaxRequests + 2, net::URLRequestTestJob::test_url_2(),
      RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loaders[kMaxRequests + 1]),
      clients[kMaxRequests + 1].CreateInterfacePtr());
  // Issue two requests for the second process -- these should succeed since
  // it is just process 0 that is saturated.
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, kMaxRequests + 3,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE,
      mojo::MakeRequest(&loaders[kMaxRequests + 2]),
      clients[kMaxRequests + 2].CreateInterfacePtr());
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, kMaxRequests + 4,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE,
      mojo::MakeRequest(&loaders[kMaxRequests + 3]),
      clients[kMaxRequests + 3].CreateInterfacePtr());
  // Flush all the pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
  }

  // Check that the first kMaxRequests succeeded.
  for (size_t i = 0; i < kMaxRequests; ++i)
    CheckSuccessfulRequest(&clients[i], net::URLRequestTestJob::test_data_2());

  // Check that the subsequent two requests (kMaxRequests + 1) and
  // (kMaxRequests + 2) were failed, since the per-process bound was reached.
  clients[kMaxRequests].RunUntilComplete();
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            clients[kMaxRequests].completion_status().error_code);
  clients[kMaxRequests + 1].RunUntilComplete();
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            clients[kMaxRequests + 1].completion_status().error_code);

  // The final 2 requests should have succeeded.
  CheckSuccessfulRequest(&clients[kMaxRequests + 2],
                         net::URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(&clients[kMaxRequests + 3],
                         net::URLRequestTestJob::test_data_2());

  second_filter->OnChannelClosing();
}

// Test that when too many requests are outstanding for a particular
// render_process_host_id, any subsequent request from it fails. Also verify
// that the global limit is honored.
TEST_P(ResourceDispatcherHostTest, TooManyOutstandingRequests) {
  // Tighten the bound on the ResourceDispatcherHost, to speed things up.
  constexpr size_t kMaxRequestsPerProcess = 2;
  host_.set_max_num_in_flight_requests_per_process(kMaxRequestsPerProcess);
  constexpr size_t kMaxRequests = 3;
  host_.set_max_num_in_flight_requests(kMaxRequests);

  // Needed to emulate additional processes.
  scoped_refptr<TestFilter> second_filter = MakeTestFilter();
  scoped_refptr<TestFilter> third_filter = MakeTestFilter();

  network::mojom::URLLoaderPtr loaders[kMaxRequests + 3];
  network::TestURLLoaderClient clients[kMaxRequests + 3];

  // Saturate the number of outstanding requests for our process.
  for (size_t i = 0; i < kMaxRequestsPerProcess; ++i) {
    MakeTestRequestWithResourceType(
        filter_.get(), 0, i + 1, net::URLRequestTestJob::test_url_2(),
        RESOURCE_TYPE_SUB_RESOURCE, mojo::MakeRequest(&loaders[i]),
        clients[i].CreateInterfacePtr());
  }

  // Issue another request for our process -- this should fail immediately.
  MakeTestRequestWithResourceType(
      filter_.get(), 0, kMaxRequestsPerProcess + 1,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE,
      mojo::MakeRequest(&loaders[kMaxRequestsPerProcess]),
      clients[kMaxRequestsPerProcess].CreateInterfacePtr());

  // Issue a request for the second process -- this should succeed, because it
  // is just process 0 that is saturated.
  MakeTestRequestWithResourceType(
      second_filter.get(), 0, kMaxRequestsPerProcess + 2,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE,
      mojo::MakeRequest(&loaders[kMaxRequestsPerProcess + 1]),
      clients[kMaxRequestsPerProcess + 1].CreateInterfacePtr());

  // Issue a request for the third process -- this should fail, because the
  // global limit has been reached.
  MakeTestRequestWithResourceType(
      third_filter.get(), 0, kMaxRequestsPerProcess + 3,
      net::URLRequestTestJob::test_url_2(), RESOURCE_TYPE_SUB_RESOURCE,
      mojo::MakeRequest(&loaders[kMaxRequestsPerProcess + 2]),
      clients[kMaxRequestsPerProcess + 2].CreateInterfacePtr());

  // Flush all the pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
  }

  for (size_t i = 0; i < kMaxRequestsPerProcess; ++i)
    CheckSuccessfulRequest(&clients[i], net::URLRequestTestJob::test_data_2());

  clients[kMaxRequestsPerProcess].RunUntilComplete();
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            clients[kMaxRequestsPerProcess].completion_status().error_code);
  clients[kMaxRequestsPerProcess + 1].RunUntilComplete();
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            clients[kMaxRequestsPerProcess + 1].completion_status().error_code);
  clients[kMaxRequestsPerProcess + 2].RunUntilComplete();
  EXPECT_EQ(net::ERR_INSUFFICIENT_RESOURCES,
            clients[kMaxRequestsPerProcess + 2].completion_status().error_code);

  second_filter->OnChannelClosing();
  third_filter->OnChannelClosing();
}

// Tests that we sniff the mime type for a simple request.
TEST_P(ResourceDispatcherHostTest, MimeSniffed) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  std::string raw_headers("HTTP/1.1 200 OK\n\n");
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"), mojo::MakeRequest(&loader),
                  client.CreateInterfacePtr());

  // Flush all pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  client.RunUntilResponseReceived();
  EXPECT_EQ("text/html", client.response_head().mime_type);
  EXPECT_TRUE(client.response_head().did_mime_sniff);
}

// Tests that we don't sniff the mime type when the server provides one.
TEST_P(ResourceDispatcherHostTest, MimeNotSniffed) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  std::string raw_headers("HTTP/1.1 200 OK\n"
                          "Content-type: image/jpeg\n\n");
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"), mojo::MakeRequest(&loader),
                  client.CreateInterfacePtr());

  // Flush all pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  client.RunUntilResponseReceived();
  EXPECT_EQ("image/jpeg", client.response_head().mime_type);
  EXPECT_FALSE(client.response_head().did_mime_sniff);
}

// Tests that we don't sniff the mime type when there is no message body.
TEST_P(ResourceDispatcherHostTest, MimeNotSniffed2) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  SetResponse("HTTP/1.1 304 Not Modified\n\n");

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"), mojo::MakeRequest(&loader),
                  client.CreateInterfacePtr());

  // Flush all pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  client.RunUntilResponseReceived();
  EXPECT_EQ("", client.response_head().mime_type);
}

TEST_P(ResourceDispatcherHostTest, MimeSniff204) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  SetResponse("HTTP/1.1 204 No Content\n\n");

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"), mojo::MakeRequest(&loader),
                  client.CreateInterfacePtr());

  // Flush all pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  client.RunUntilResponseReceived();
  EXPECT_EQ("text/plain", client.response_head().mime_type);
}

TEST_P(ResourceDispatcherHostTest, MimeSniffEmpty) {
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  SetResponse("HTTP/1.1 200 OK\n\n");

  HandleScheme("http");
  MakeTestRequest(0, 1, GURL("http:bla"), mojo::MakeRequest(&loader),
                  client.CreateInterfacePtr());

  // Flush all pending requests.
  content::RunAllTasksUntilIdle();
  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  client.RunUntilResponseReceived();
  EXPECT_EQ("text/plain", client.response_head().mime_type);
}

// Tests for crbug.com/31266 (Non-2xx + application/octet-stream).
TEST_P(ResourceDispatcherHostTest, ForbiddenDownload) {
  std::string raw_headers("HTTP/1.1 403 Forbidden\n"
                          "Content-disposition: attachment; filename=blah\n"
                          "Content-type: application/octet-stream\n\n");
  std::string response_data("<html><title>Test One</title></html>");
  SetResponse(raw_headers, response_data);

  HandleScheme("http");

  int expected_error_code = net::ERR_INVALID_RESPONSE;
  GURL forbidden_download_url = GURL("http:bla");

  CompleteFailingMainResourceRequest(forbidden_download_url,
                                     expected_error_code);
}

TEST_P(ResourceDispatcherHostTest, CancelRequestsForContextDetached) {
  EXPECT_EQ(0, host_.pending_requests());
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;
  constexpr int render_view_id = 0;
  constexpr int request_id = 1;

  MakeTestRequestWithResourceType(filter_.get(), render_view_id, request_id,
                                  net::URLRequestTestJob::test_url_4(),
                                  RESOURCE_TYPE_PREFETCH,  // detachable type
                                  mojo::MakeRequest(&loader),
                                  client.CreateInterfacePtr());

  // Simulate a cancel coming from the renderer.
  loader = nullptr;
  content::RunAllTasksUntilIdle();

  // Since the request had already started processing as detachable,
  // the cancellation above should have been ignored and the request
  // should have been detached.
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by other methods should also leave it detached.
  host_.CancelRequestsForProcess(render_view_id);
  EXPECT_EQ(1, host_.pending_requests());

  // Cancelling by context should work.
  host_.CancelRequestsForContext(filter_->resource_context());
  EXPECT_EQ(0, host_.pending_requests());
}

namespace {

class ExternalProtocolBrowserClient : public TestContentBrowserClient {
 public:
  ExternalProtocolBrowserClient() {}

  bool HandleExternalProtocol(
      const GURL& url,
      ResourceRequestInfo::WebContentsGetter web_contents_getter,
      int child_id,
      NavigationUIData* navigation_data,
      bool is_main_frame,
      ui::PageTransition page_transition,
      bool has_user_gesture) override {
    return false;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ExternalProtocolBrowserClient);
};

}  // namespace

// Verifies that if the embedder says that it didn't handle an unkonown protocol
// the request is cancelled and net::ERR_ABORTED is returned. Otherwise it is
// not aborted and net/ layer cancels it with net::ERR_UNKNOWN_URL_SCHEME.
TEST_P(ResourceDispatcherHostTest, UnknownURLScheme) {
  EXPECT_EQ(0, host_.pending_requests());

  HandleScheme("http");

  ExternalProtocolBrowserClient test_client;
  ContentBrowserClient* old_client = SetBrowserClientForTesting(&test_client);

  const GURL invalid_scheme_url = GURL("foo://bar");
  int expected_error_code = net::ERR_UNKNOWN_URL_SCHEME;
  CompleteFailingMainResourceRequest(invalid_scheme_url, expected_error_code);
  SetBrowserClientForTesting(old_client);

  expected_error_code = net::ERR_ABORTED;
  CompleteFailingMainResourceRequest(invalid_scheme_url, expected_error_code);
}

// Request a very large detachable resource and cancel part way. Some of the
// data should have been sent to the renderer, but not all.
TEST_P(ResourceDispatcherHostTest, DataSentBeforeDetach) {
  EXPECT_EQ(0, host_.pending_requests());

  constexpr int render_view_id = 0;
  constexpr int request_id = 1;
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;

  std::string raw_headers("HTTP\n"
                          "Content-type: image/jpeg\n\n");
  std::string response_data("01234567890123456789\x01foobar");

  // Create a response larger than kMaxAllocationSize (currently 32K). Note
  // that if this increase beyond 512K we'll need to make the response longer.
  const int kAllocSize = 1024*512;
  response_data.resize(kAllocSize, ' ');

  SetResponse(raw_headers, response_data);
  job_factory_->SetDelayedCompleteJobGeneration(true);
  HandleScheme("http");

  MakeTestRequestWithResourceType(
      filter_.get(), render_view_id, request_id,
      GURL("http://example.com/blah"), RESOURCE_TYPE_PREFETCH,
      mojo::MakeRequest(&loader), client.CreateInterfacePtr());
  content::RunAllTasksUntilIdle();

  // Get a bit of data before cancelling.
  EXPECT_TRUE(net::URLRequestTestJob::ProcessOnePendingMessage());

  // Simulate a cancellation coming from the renderer.
  loader = nullptr;
  content::RunAllTasksUntilIdle();

  EXPECT_EQ(1, host_.pending_requests());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {}

  // Verify the data that was received before cancellation. The request should
  // have appeared to cancel, however.
  client.RunUntilComplete();
  EXPECT_TRUE(client.response_body().is_valid());
  EXPECT_EQ(net::ERR_ABORTED, client.completion_status().error_code);
}

WebContents* WebContentsBinder(WebContents* rv) { return rv; }

// Tests GetLoadInfoForAllRoutes when there are 3 requests from the same
// RenderView.  The second one is farthest along.
TEST_P(ResourceDispatcherHostTest, LoadInfo) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_SENDING_REQUEST,
                                            base::string16());
  info.host = "a.com";
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  info.host = "b.com";
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_READING_RESPONSE,
                                            base::string16());
  infos->push_back(info);

  info.host = "c.com";

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(1u, load_info_map->size());
  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ("b.com", (*load_info_map)[wc1].host);
  EXPECT_EQ(net::LOAD_STATE_READING_RESPONSE,
            (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_size);
}

// Tests GetLoadInfoForAllRoutes when there are 2 requests with the same
// priority.  The first one (Which will have the lowest ID) should be returned.
TEST_P(ResourceDispatcherHostTest, LoadInfoSamePriority) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_IDLE,
                                            base::string16());
  info.host = "a.com";
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  info.host = "b.com";
  infos->push_back(info);

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(1u, load_info_map->size());
  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ("a.com", (*load_info_map)[wc1].host);
  EXPECT_EQ(net::LOAD_STATE_IDLE, (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_size);
}

// Tests GetLoadInfoForAllRoutes when a request is uploading a body.
TEST_P(ResourceDispatcherHostTest, LoadInfoUploadProgress) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_READING_RESPONSE,
                                            base::string16());
  info.host = "a.com";
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  info.upload_position = 1000;
  info.upload_size = 1000;
  infos->push_back(info);

  info.host = "b.com";
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_SENDING_REQUEST,
                                            base::string16());
  info.upload_position = 50;
  info.upload_size = 100;
  infos->push_back(info);

  info.host = "a.com";
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_READING_RESPONSE,
                                            base::string16());
  info.upload_position = 1000;
  info.upload_size = 1000;
  infos->push_back(info);

  info.host = "c.com";
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(1u, load_info_map->size());
  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ("b.com", (*load_info_map)[wc1].host);
  EXPECT_EQ(net::LOAD_STATE_SENDING_REQUEST,
            (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(50u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(100u, (*load_info_map)[wc1].upload_size);
}

// Tests GetLoadInfoForAllRoutes when there are 4 requests from 2 different
// RenderViews.  Also tests the case where the first / last requests are the
// most interesting ones.
TEST_P(ResourceDispatcherHostTest, LoadInfoTwoRenderViews) {
  std::unique_ptr<LoadInfoList> infos(new LoadInfoList);
  LoadInfo info;
  WebContents* wc1 = reinterpret_cast<WebContents*>(0x1);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_CONNECTING,
                                            base::string16());
  info.host = "a.com";
  info.upload_position = 0;
  info.upload_size = 0;
  infos->push_back(info);

  WebContents* wc2 = reinterpret_cast<WebContents*>(0x2);
  info.web_contents_getter = base::Bind(WebContentsBinder, wc2);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_IDLE,
                                            base::string16());
  info.host = "b.com";
  infos->push_back(info);

  info.web_contents_getter = base::Bind(WebContentsBinder, wc1);
  info.host = "c.com";
  infos->push_back(info);

  info.web_contents_getter = base::Bind(WebContentsBinder, wc2);
  info.load_state = net::LoadStateWithParam(net::LOAD_STATE_CONNECTING,
                                            base::string16());
  info.host = "d.com";
  infos->push_back(info);

  std::unique_ptr<LoadInfoMap> load_info_map =
      ResourceDispatcherHostImpl::PickMoreInterestingLoadInfos(
          std::move(infos));
  ASSERT_EQ(2u, load_info_map->size());

  ASSERT_TRUE(load_info_map->find(wc1) != load_info_map->end());
  EXPECT_EQ("a.com", (*load_info_map)[wc1].host);
  EXPECT_EQ(net::LOAD_STATE_CONNECTING,
            (*load_info_map)[wc1].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc1].upload_size);

  ASSERT_TRUE(load_info_map->find(wc2) != load_info_map->end());
  EXPECT_EQ("d.com", (*load_info_map)[wc2].host);
  EXPECT_EQ(net::LOAD_STATE_CONNECTING,
            (*load_info_map)[wc2].load_state.state);
  EXPECT_EQ(0u, (*load_info_map)[wc2].upload_position);
  EXPECT_EQ(0u, (*load_info_map)[wc2].upload_size);
}

// Tests that a ResourceThrottle that needs to process the response before any
// part of the body is read can do so.
TEST_P(ResourceDispatcherHostTest, ThrottleMustProcessResponseBeforeRead) {
  // Ensure all jobs will check that no read operation is called.
  job_factory_->SetMustNotReadJobGeneration(true);
  HandleScheme("http");

  // Create a ResourceThrottle that must process the response before any part of
  // the body is read. This throttle will also cancel the request in
  // WillProcessResponse.
  TestResourceDispatcherHostDelegate delegate;
  int throttle_flags = CANCEL_PROCESSING_RESPONSE | MUST_NOT_CACHE_BODY;
  delegate.set_flags(throttle_flags);
  host_.SetDelegate(&delegate);

  // This response should normally result in the MIME type being sniffed, which
  // requires reading the body.
  std::string raw_headers(
      "HTTP/1.1 200 OK\n"
      "Content-Type: text/plain; charset=utf-8\n\n");
  std::string response_data("p { text-align: center; }");
  SetResponse(raw_headers, response_data);
  network::mojom::URLLoaderPtr loader;
  network::TestURLLoaderClient client;

  MakeTestRequestWithResourceType(
      filter_.get(), filter_->child_id(), 1, GURL("http://example.com/blah"),
      RESOURCE_TYPE_STYLESHEET, mojo::MakeRequest(&loader),
      client.CreateInterfacePtr());

  while (net::URLRequestTestJob::ProcessOnePendingMessage()) {
  }
  content::RunAllTasksUntilIdle();
}

// A URLRequestTestJob that sets a test certificate on the |ssl_info|
// field of the response.
class TestHTTPSURLRequestJob : public net::URLRequestTestJob {
 public:
  TestHTTPSURLRequestJob(net::URLRequest* request,
                         net::NetworkDelegate* network_delegate,
                         const std::string& response_headers,
                         const std::string& response_data,
                         bool auto_advance)
      : net::URLRequestTestJob(request,
                               network_delegate,
                               response_headers,
                               response_data,
                               auto_advance) {}

  void GetResponseInfo(net::HttpResponseInfo* info) override {
    net::URLRequestTestJob::GetResponseInfo(info);
    info->ssl_info.cert =
        net::ImportCertFromFile(net::GetTestCertsDirectory(), "ok_cert.pem");
  }
};

net::URLRequestJob* TestURLRequestJobFactory::MaybeCreateJobWithProtocolHandler(
      const std::string& scheme,
      net::URLRequest* request,
      net::NetworkDelegate* network_delegate) const {
  url_request_jobs_created_count_++;
  if (hang_after_start_) {
    return new net::URLRequestFailedJob(request, network_delegate,
                                        net::ERR_IO_PENDING);
  }
  if (test_fixture_->response_headers_.empty()) {
    if (delay_start_) {
      return new URLRequestTestDelayedStartJob(request, network_delegate);
    } else if (delay_complete_) {
      return new URLRequestTestDelayedCompletionJob(request,
                                                    network_delegate);
    } else if (scheme == "big-job") {
      return new URLRequestBigJob(request, network_delegate);
    } else {
      return new net::URLRequestTestJob(request, network_delegate,
                                        test_fixture_->auto_advance_);
    }
  } else {
    if (delay_start_) {
      return new URLRequestTestDelayedStartJob(
          request, network_delegate,
          test_fixture_->response_headers_, test_fixture_->response_data_,
          false);
    } else if (delay_complete_) {
      return new URLRequestTestDelayedCompletionJob(
          request, network_delegate,
          test_fixture_->response_headers_, test_fixture_->response_data_,
          false);
    } else if (must_not_read_) {
      return new URLRequestMustNotReadTestJob(request, network_delegate,
                                              test_fixture_->response_headers_,
                                              test_fixture_->response_data_);
    } else if (test_fixture_->use_test_ssl_certificate_) {
      return new TestHTTPSURLRequestJob(request, network_delegate,
                                        test_fixture_->response_headers_,
                                        test_fixture_->response_data_, false);
    } else {
      return new net::URLRequestTestJob(
          request, network_delegate, test_fixture_->response_headers_,
          test_fixture_->response_data_, test_fixture_->auto_advance_);
    }
  }
}

net::URLRequestJob* TestURLRequestJobFactory::MaybeInterceptRedirect(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate,
    const GURL& location) const {
  return nullptr;
}

net::URLRequestJob* TestURLRequestJobFactory::MaybeInterceptResponse(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate) const {
  return nullptr;
}

INSTANTIATE_TEST_CASE_P(WithoutOutOfBlinkCors,
                        ResourceDispatcherHostTest,
                        ::testing::Values(TestMode::kWithoutOutOfBlinkCors));

INSTANTIATE_TEST_CASE_P(WithOutOfBlinkCors,
                        ResourceDispatcherHostTest,
                        ::testing::Values(TestMode::kWithOutOfBlinkCors));

}  // namespace content
