// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_isolated_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_partition_key_collection.h"

namespace content {
namespace {

PrefetchSingleRedirectHop::OnIsolatedCookieCopyStartCallbackForTesting&
GetOnIsolatedCookieCopyStartCallbackForTesting() {
  static base::NoDestructor<
      PrefetchSingleRedirectHop::OnIsolatedCookieCopyStartCallbackForTesting>
      on_isolated_cookie_copy_start_callback_for_testing;
  return *on_isolated_cookie_copy_start_callback_for_testing;
}

void RecordCookieCopyTimes(
    const base::TimeTicks& cookie_copy_start_time,
    const base::TimeTicks& cookie_read_end_and_write_start_time,
    const base::TimeTicks& cookie_copy_end_time) {
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieReadTime",
      cookie_read_end_and_write_start_time - cookie_copy_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieWriteTime",
      cookie_copy_end_time - cookie_read_end_and_write_start_time,
      base::TimeDelta(), base::Seconds(5), 50);
  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyTime",
      cookie_copy_end_time - cookie_copy_start_time, base::TimeDelta(),
      base::Seconds(5), 50);
}

void RecordPrefetchProxyPrefetchMainframeCookiesToCopy(
    size_t cookie_list_size) {
  UMA_HISTOGRAM_COUNTS_100("PrefetchProxy.Prefetch.Mainframe.CookiesToCopy",
                           cookie_list_size);
}

}  // namespace

PrefetchSingleRedirectHop::PrefetchSingleRedirectHop(
    PrefetchContainer& prefetch_container,
    const GURL& url,
    perfetto::Flow flow)
    : url_(url),
      is_isolated_network_context_required_(
          prefetch_container.request().IsIsolatedNetworkContextRequired(url_)),
      response_reader_(base::MakeRefCounted<PrefetchResponseReader>(
          base::BindOnce(&PrefetchContainer::OnDeterminedHead,
                         prefetch_container.GetWeakPtr()),
          base::BindOnce(&PrefetchContainer::OnPrefetchComplete,
                         prefetch_container.GetWeakPtr()),
          std::move(flow))),
      prefetch_container_(prefetch_container) {}

PrefetchSingleRedirectHop::~PrefetchSingleRedirectHop() {
  CHECK(response_reader_);
  base::SequencedTaskRunner::GetCurrentDefault()->ReleaseSoon(
      FROM_HERE, std::move(response_reader_));
}

void PrefetchSingleRedirectHop::RegisterCookieListener() {
  if (!is_isolated_network_context_required()) {
    return;
  }

  cookie_listener_ = PrefetchCookieListener::MakeAndRegister(
      url_, prefetch_container_->request()
                .browser_context()
                ->GetDefaultStoragePartition()
                ->GetCookieManagerForBrowserProcess());
}

bool PrefetchSingleRedirectHop::HaveDefaultContextCookiesChanged() const {
  if (cookie_listener_) {
    return cookie_listener_->HaveCookiesChanged();
  }
  return false;
}

void PrefetchSingleRedirectHop::PauseCookieListener() {
  if (cookie_listener_) {
    cookie_listener_->PauseListening();
  }
}

void PrefetchSingleRedirectHop::ResumeCookieListener() {
  if (cookie_listener_) {
    cookie_listener_->ResumeListening();
  }
}

bool PrefetchSingleRedirectHop::HasIsolatedCookieCopyStarted() const {
  switch (cookie_copy_status_) {
    case CookieCopyStatus::kNotStarted:
      return false;
    case CookieCopyStatus::kInProgress:
    case CookieCopyStatus::kCompleted:
      return true;
  }
}

bool PrefetchSingleRedirectHop::IsIsolatedCookieCopyInProgress() const {
  switch (cookie_copy_status_) {
    case CookieCopyStatus::kNotStarted:
    case CookieCopyStatus::kCompleted:
      return false;
    case CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchSingleRedirectHop::
    SetOnIsolatedCookieCopyStartCallbackForTesting(  // IN-TEST
        PrefetchSingleRedirectHop::OnIsolatedCookieCopyStartCallbackForTesting
            on_isolated_cookie_copy_start_callback_for_testing) {
  GetOnIsolatedCookieCopyStartCallbackForTesting() =  // IN-TEST
      std::move(on_isolated_cookie_copy_start_callback_for_testing);
}

void PrefetchSingleRedirectHop::OnIsolatedCookieCopyStart() {
  DCHECK(!IsIsolatedCookieCopyInProgress());

  // We should temporarily ignore the cookie monitoring by
  // `PrefetchCookieListener` during the isolated cookie is written to the
  // default network context.
  // `PrefetchCookieListener` should monitor whether the cookie is changed from
  // what we stored in isolated network context when prefetching so that we can
  // avoid serving the stale prefetched content. Currently
  // `PrefetchCookieListener` will also catch isolated cookie copy as a cookie
  // change. To handle this event as a false positive (as the cookie isn't
  // changed from what we stored on prefetching), we can pause the lisner during
  // copying, keeping the prefetch servable.
  prefetch_container_->PauseAllCookieListeners();

  cookie_copy_status_ = CookieCopyStatus::kInProgress;
  cookie_copy_start_time_ = base::TimeTicks::Now();

  if (GetOnIsolatedCookieCopyStartCallbackForTesting()) {
    GetOnIsolatedCookieCopyStartCallbackForTesting().Run(  // IN-TEST
        prefetch_container_->GetURL(), url_);
  }
}

void PrefetchSingleRedirectHop::OnIsolatedCookiesReadCompleteAndWriteStart() {
  DCHECK(IsIsolatedCookieCopyInProgress());
  cookie_read_end_and_write_start_time_ = base::TimeTicks::Now();
}

void PrefetchSingleRedirectHop::CopyIsolatedCookies() {
  // We only need to copy cookies if the prefetch used an isolated network
  // context.
  if (!is_isolated_network_context_required()) {
    return;
  }

  if (HasIsolatedCookieCopyStarted()) {
    return;
  }

  OnIsolatedCookieCopyStart();

  PrefetchIsolatedNetworkContext* isolated_network_context =
      prefetch_container_->GetIsolatedNetworkContext();

  if (!isolated_network_context) {
    CHECK_IS_TEST();
    // Not set in unit tests. In non-test cases, `isolated_network_context` is
    // always non-null because the isolated network context should have been
    // created at the time of prefetch.
    return;
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  isolated_network_context->GetCookieManager()->GetCookieList(
      url_, options, net::CookiePartitionKeyCollection::Todo(),
      base::BindOnce(&PrefetchSingleRedirectHop::OnGotIsolatedCookiesForCopy,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PrefetchSingleRedirectHop::OnGotIsolatedCookiesForCopy(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) {
  OnIsolatedCookiesReadCompleteAndWriteStart();

  RecordPrefetchProxyPrefetchMainframeCookiesToCopy(cookie_list.size());

  if (cookie_list.empty()) {
    OnIsolatedCookieCopyComplete();
    return;
  }

  network::mojom::CookieManager* default_cookie_manager =
      prefetch_container_->request()
          .browser_context()
          ->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();

  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_list.size(),
      base::BindOnce(&PrefetchSingleRedirectHop::OnIsolatedCookieCopyComplete,
                     weak_ptr_factory_.GetWeakPtr()));

  // Do not touch `this` below, because `this` is already moved out here.

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  for (const net::CookieWithAccessResult& cookie : cookie_list) {
    default_cookie_manager->SetCanonicalCookie(
        cookie.cookie, url_, options,
        base::BindOnce(
            [](base::RepeatingClosure closure,
               net::CookieAccessResult access_result) { closure.Run(); },
            barrier));
  }
}

void PrefetchSingleRedirectHop::OnIsolatedCookieCopyComplete() {
  DCHECK(IsIsolatedCookieCopyInProgress());

  // Resumes `PrefetchCookieListener` so that we can keep monitoring the
  // cookie change for the prefetch, which may be served again.
  prefetch_container_->ResumeAllCookieListeners();

  cookie_copy_status_ = PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted;

  if (cookie_copy_start_time_.has_value() &&
      cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(cookie_copy_start_time_.value(),
                          cookie_read_end_and_write_start_time_.value(),
                          base::TimeTicks::Now());
  }

  if (on_cookie_copy_complete_callback_) {
    std::move(on_cookie_copy_complete_callback_).Run();
  }
}

void PrefetchSingleRedirectHop::OnInterceptorCheckCookieCopy() {
  if (!cookie_copy_start_time_) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() - cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

void PrefetchSingleRedirectHop::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  DCHECK(IsIsolatedCookieCopyInProgress());
  on_cookie_copy_complete_callback_ = std::move(callback);
}

}  // namespace content
