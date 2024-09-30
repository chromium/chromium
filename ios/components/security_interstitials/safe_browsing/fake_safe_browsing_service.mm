// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"

#import "base/functional/callback_helpers.h"
#import "base/memory/raw_ptr.h"
#import "base/task/sequenced_task_runner.h"
#import "components/safe_browsing/core/browser/db/test_database_manager.h"
#import "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_client.h"
#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_state.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace {

// This is used to vend a RepeatingCallback which runs the
// NativeCheckUrlCallback on only the first run.
class CheckUrlCallbackRunner {
 public:
  CheckUrlCallbackRunner(
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback
          callback)
      : callback_(std::move(callback)) {}
  ~CheckUrlCallbackRunner() = default;

  void MaybeRunCallback(
      security_interstitials::UnsafeResource::UrlCheckResult result) {
    if (callback_) {
      std::move(callback_).Run(result.proceed, result.showed_interstitial,
                               result.has_post_commit_interstitial_skipped,
                               /*did_perform_url_real_time_check=*/
                               safe_browsing::SafeBrowsingUrlCheckerImpl::
                                   PerformedCheck::kHashDatabaseCheck);
    }
  }

 private:
  safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback callback_;
};

void RunCheckUrlCallback(
    const GURL& url,
    bool is_url_unsafe,
    safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback callback,
    const std::string& method,
    scoped_refptr<safe_browsing::UrlCheckerDelegate> url_checker_delegate,
    base::WeakPtr<web::WebState> web_state) {
  if (is_url_unsafe) {
    security_interstitials::UnsafeResource resource;
    resource.url = url;
    resource.threat_type =
        safe_browsing::SBThreatType::SB_THREAT_TYPE_URL_PHISHING;
    resource.threat_source = safe_browsing::ThreatSource::LOCAL_PVER4;
    resource.callback = base::BindRepeating(
        &CheckUrlCallbackRunner::MaybeRunCallback,
        std::make_unique<CheckUrlCallbackRunner>(std::move(callback)));
    resource.callback_sequence = base::SequencedTaskRunner::GetCurrentDefault();
    resource.weak_web_state = web_state;

    // Notify the `url_checker_delegate`, which will cause `callback` to
    // be run asynchronously after the `UnsafeResource` has been saved.
    url_checker_delegate->StartDisplayingBlockingPageHelper(
        std::move(resource), method, net::HttpRequestHeaders(),
        /*has_user_gesture=*/false);
    return;
  }
  std::move(callback).Run(/*proceed=*/true,
                          /*showed_interstitial=*/false,
                          /*has_post_commit_interstitial_skipped=*/false,
                          /*did_perform_url_real_time_check=*/
                          safe_browsing::SafeBrowsingUrlCheckerImpl::
                              PerformedCheck::kHashDatabaseCheck);
}

// A SafeBrowsingUrlCheckerImpl that treats all URLs as safe, unless they have
// host safe.browsing.unsafe.chromium.test.
class FakeSafeBrowsingUrlCheckerImpl
    : public safe_browsing::SafeBrowsingUrlCheckerImpl {
 public:
  FakeSafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination,
      base::WeakPtr<web::WebState> web_state)
      : SafeBrowsingUrlCheckerImpl(
            /*headers=*/net::HttpRequestHeaders(),
            /*load_flags=*/0,
            /*has_user_gesture=*/false,
            base::MakeRefCounted<UrlCheckerDelegateImpl>(
                /*database_manager=*/nullptr,
                /*client=*/nullptr), /*web_contents_getter=*/
            base::RepeatingCallback<content::WebContents*()>(),
            web_state,
            /*render_process_id=*/
            security_interstitials::UnsafeResource::kNoRenderProcessId,
            /*render_frame_token=*/std::nullopt,
            /*frame_tree_node_id=*/
            security_interstitials::UnsafeResource::kNoFrameTreeNodeId,
            /*navigation_id=*/std::nullopt,
            /*url_real_time_lookup_enabled=*/false,
            /*can_check_db=*/true,
            /*can_check_high_confidence_allowlist=*/true,
            /*url_lookup_service_metric_suffix=*/"",
            web::GetUIThreadTaskRunner({}),
            /*url_lookup_service_on_ui=*/nullptr,
            /*hash_realtime_service_on_ui=*/nullptr,
            safe_browsing::hash_realtime_utils::HashRealTimeSelection::kNone,
            /*is_async_check=*/false,
            /*check_allowlist_before_hash_database=*/false,
            SessionID::InvalidValue()) {}

  FakeSafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination,
      base::WeakPtr<web::WebState> web_state,
      FakeSafeBrowsingClient* client,
      bool is_async_check)
      : FakeSafeBrowsingUrlCheckerImpl(request_destination, web_state) {
    client_ = client;
    is_async_check_ = is_async_check;
  }

  ~FakeSafeBrowsingUrlCheckerImpl() override = default;

  // SafeBrowsingUrlCheckerImpl:
  void CheckUrl(
      const GURL& url,
      const std::string& method,
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback
          callback) override {
    if (client_) {
      if (is_async_check_) {
        client_->store_async_callback(base::BindOnce(
            &RunCheckUrlCallback, url, IsUrlUnsafe(url), std::move(callback),
            method, url_checker_delegate(), web_state()));
      } else {
        client_->store_sync_callback(base::BindOnce(
            &RunCheckUrlCallback, url, IsUrlUnsafe(url), std::move(callback),
            method, url_checker_delegate(), web_state()));
      }
    } else {
      // Always respond asynchronously to support tests in
      // safe_browsing_tab_helper_unittest.mm
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(&RunCheckUrlCallback, url, IsUrlUnsafe(url),
                                    std::move(callback), method,
                                    url_checker_delegate(), web_state()));
    }
  }

 protected:
  // Returns true if the given `url` should be deemed unsafe.
  virtual bool IsUrlUnsafe(const GURL& url) {
    return url.host() == FakeSafeBrowsingService::kUnsafeHost;
  }

  raw_ptr<FakeSafeBrowsingClient> client_ = nullptr;
  bool is_async_check_ = false;
};

// A SafeBrowsingUrlCheckerImpl that treats all URLs as safe, unless they have
// host safe.browsing.unsafe.chromium.test or
// safe.browsing.async.unsafe.chromium.test.
class FakeAsyncSafeBrowsingUrlCheckerImpl
    : public FakeSafeBrowsingUrlCheckerImpl {
 public:
  explicit FakeAsyncSafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination,
      base::WeakPtr<web::WebState> web_state,
      FakeSafeBrowsingClient* client,
      bool is_async_check)
      : FakeSafeBrowsingUrlCheckerImpl(request_destination,
                                       web_state,
                                       client,
                                       is_async_check) {}

 protected:
  bool IsUrlUnsafe(const GURL& url) override {
    if (url.host() == FakeSafeBrowsingService::kAsyncUnsafeHost) {
      return true;
    }
    return FakeSafeBrowsingUrlCheckerImpl::IsUrlUnsafe(url);
  }
};

}  // namespace

// static
const std::string FakeSafeBrowsingService::kUnsafeHost =
    "safe.browsing.unsafe.chromium.test";
const std::string FakeSafeBrowsingService::kAsyncUnsafeHost =
    "safe.browsing.async.unsafe.chromium.test";

FakeSafeBrowsingService::FakeSafeBrowsingService() = default;

FakeSafeBrowsingService::~FakeSafeBrowsingService() = default;

void FakeSafeBrowsingService::Initialize(
    PrefService* prefs,
    const base::FilePath& user_data_path,
    safe_browsing::SafeBrowsingMetricsCollector*
        safe_browsing_metrics_collector) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

void FakeSafeBrowsingService::ShutDown() {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
FakeSafeBrowsingService::CreateUrlChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  return std::make_unique<FakeSafeBrowsingUrlCheckerImpl>(
      request_destination, web_state->GetWeakPtr());
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
FakeSafeBrowsingService::CreateSyncChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  auto* test_client = static_cast<FakeSafeBrowsingClient*>(client);
  return std::make_unique<FakeSafeBrowsingUrlCheckerImpl>(
      request_destination, web_state->GetWeakPtr(), test_client,
      /*is_async_check=*/false);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
FakeSafeBrowsingService::CreateAsyncChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  auto* test_client = static_cast<FakeSafeBrowsingClient*>(client);
  return std::make_unique<FakeAsyncSafeBrowsingUrlCheckerImpl>(
      request_destination, web_state->GetWeakPtr(), test_client,
      /*is_async_check=*/true);
}

bool FakeSafeBrowsingService::ShouldCreateAsyncChecker(
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  if (!base::FeatureList::IsEnabled(
          safe_browsing::kSafeBrowsingAsyncRealTimeCheck)) {
    return false;
  }

  return true;
}

bool FakeSafeBrowsingService::CanCheckUrl(const GURL& url) const {
  return url.SchemeIsHTTPOrHTTPS() || url.SchemeIs(url::kFtpScheme) ||
         url.SchemeIsWSOrWSS();
}

scoped_refptr<network::SharedURLLoaderFactory>
FakeSafeBrowsingService::GetURLLoaderFactory() {
  return base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
      &url_loader_factory_);
}

scoped_refptr<safe_browsing::SafeBrowsingDatabaseManager>
FakeSafeBrowsingService::GetDatabaseManager() {
  return nullptr;
}

network::mojom::NetworkContext* FakeSafeBrowsingService::GetNetworkContext() {
  return nullptr;
}

void FakeSafeBrowsingService::ClearCookies(
    const net::CookieDeletionInfo::TimeRange& creation_range,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  std::move(callback).Run();
}
