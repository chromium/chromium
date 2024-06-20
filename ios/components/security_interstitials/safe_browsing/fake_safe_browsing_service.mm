// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"

#import "base/functional/callback_helpers.h"
#import "components/safe_browsing/core/browser/db/test_database_manager.h"
#import "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "components/safe_browsing/core/common/features.h"
#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

namespace {
// A SafeBrowsingUrlCheckerImpl that treats all URLs as safe, unless they have
// host safe.browsing.unsafe.chromium.test.
class FakeSafeBrowsingUrlCheckerImpl
    : public safe_browsing::SafeBrowsingUrlCheckerImpl {
 public:
  explicit FakeSafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination)
      : SafeBrowsingUrlCheckerImpl(
            /*headers=*/net::HttpRequestHeaders(),
            /*load_flags=*/0,
            /*has_user_gesture=*/false,
            base::MakeRefCounted<UrlCheckerDelegateImpl>(
                /*database_manager=*/nullptr,
                /*client=*/nullptr), /*web_contents_getter=*/
            base::RepeatingCallback<content::WebContents*()>(),
            base::WeakPtr<web::WebState>(),
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
            SessionID::InvalidValue()) {}
  ~FakeSafeBrowsingUrlCheckerImpl() override = default;

  // SafeBrowsingUrlCheckerImpl:
  void CheckUrl(
      const GURL& url,
      const std::string& method,
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback
          callback) override {
    if (url.host() == FakeSafeBrowsingService::kUnsafeHost) {
      std::move(callback).Run(
          /*proceed=*/false,
          /*showed_interstitial=*/true,
          /*has_post_commit_interstitial_skipped=*/false,
          /*did_perform_url_real_time_check=*/
          safe_browsing::SafeBrowsingUrlCheckerImpl::PerformedCheck::
              kHashDatabaseCheck);
      return;
    }
    std::move(callback).Run(/*proceed=*/true,
                            /*showed_interstitial=*/false,
                            /*has_post_commit_interstitial_skipped=*/false,
                            /*did_perform_url_real_time_check=*/
                            safe_browsing::SafeBrowsingUrlCheckerImpl::
                                PerformedCheck::kHashDatabaseCheck);
  }
};
}  // namespace

// static
const std::string FakeSafeBrowsingService::kUnsafeHost =
    "safe.browsing.unsafe.chromium.test";

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
  return std::make_unique<FakeSafeBrowsingUrlCheckerImpl>(request_destination);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
FakeSafeBrowsingService::CreateSyncChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  return std::make_unique<FakeSafeBrowsingUrlCheckerImpl>(request_destination);
}

std::unique_ptr<safe_browsing::SafeBrowsingUrlCheckerImpl>
FakeSafeBrowsingService::CreateAsyncChecker(
    network::mojom::RequestDestination request_destination,
    web::WebState* web_state,
    SafeBrowsingClient* client) {
  return std::make_unique<FakeSafeBrowsingUrlCheckerImpl>(request_destination);
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
