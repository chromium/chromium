// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/components/security_interstitials/safe_browsing/fake_safe_browsing_service.h"

#import "base/callback_helpers.h"
#import "components/safe_browsing/core/browser/db/test_database_manager.h"
#import "components/safe_browsing/core/browser/safe_browsing_url_checker_impl.h"
#import "ios/components/security_interstitials/safe_browsing/url_checker_delegate_impl.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// A SafeBrowsingUrlCheckerImpl that treats all URLs as safe, unless they have
// host safe.browsing.unsafe.chromium.test.
class FakeSafeBrowsingUrlCheckerImpl
    : public safe_browsing::SafeBrowsingUrlCheckerImpl {
 public:
  explicit FakeSafeBrowsingUrlCheckerImpl(
      network::mojom::RequestDestination request_destination)
      : SafeBrowsingUrlCheckerImpl(request_destination,
                                   base::MakeRefCounted<UrlCheckerDelegateImpl>(
                                       /*database_manager=*/nullptr,
                                       /*client=*/nullptr),
                                   base::WeakPtr<web::WebState>(),
                                   /*real_time_lookup_enabled=*/false,
                                   /*can_rt_check_subresource_url=*/false,
                                   web::GetUIThreadTaskRunner({}),
                                   /*url_lookup_service_on_ui=*/nullptr) {}
  ~FakeSafeBrowsingUrlCheckerImpl() override = default;

  // SafeBrowsingUrlCheckerImpl:
  void CheckUrl(
      const GURL& url,
      const std::string& method,
      safe_browsing::SafeBrowsingUrlCheckerImpl::NativeCheckUrlCallback
          callback) override {
    if (url.host() == FakeSafeBrowsingService::kUnsafeHost) {
      std::move(callback).Run(/*slow_check_notifier=*/nullptr,
                              /*proceed=*/false,
                              /*showed_interstitial=*/true,
                              /*did_perform_real_time_check=*/false,
                              /*did_check_allowlist=*/true);
      return;
    }
    std::move(callback).Run(/*slow_check_notifier=*/nullptr, /*proceed=*/true,
                            /*showed_interstitial=*/false,
                            /*did_perform_real_time_check=*/false,
                            /*did_check_allowlist=*/true);
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
  return nil;
}

void FakeSafeBrowsingService::ClearCookies(
    const net::CookieDeletionInfo::TimeRange& creation_range,
    base::OnceClosure callback) {
  DCHECK_CURRENTLY_ON(web::WebThread::UI);
  std::move(callback).Run();
}
