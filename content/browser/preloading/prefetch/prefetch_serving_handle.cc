// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/preloading/prefetch/prefetch_serving_handle.h"

#include "base/barrier_closure.h"
#include "base/check_is_test.h"
#include "base/metrics/histogram_macros.h"
#include "base/notimplemented.h"
#include "base/time/time.h"
#include "content/browser/preloading/prefetch/prefetch_container.h"
#include "content/browser/preloading/prefetch/prefetch_cookie_listener.h"
#include "content/browser/preloading/prefetch/prefetch_network_context.h"
#include "content/browser/preloading/prefetch/prefetch_probe_result.h"
#include "content/browser/preloading/prefetch/prefetch_request.h"
#include "content/browser/preloading/prefetch/prefetch_response_reader.h"
#include "content/browser/preloading/prefetch/prefetch_single_redirect_hop.h"
#include "content/browser/preloading/prefetch/prefetch_status.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/preloading.h"
#include "content/public/browser/storage_partition.h"
#include "url/gurl.h"

namespace content {
namespace {

// Helper for `base::BindOnce()` + rvalue ref-qualified member method.
template <typename... UnboundArgs,
          typename Method,
          typename Receiver,
          typename... BoundArgs>
auto BindOnceForRvalueMemberMethod(Method method,
                                   Receiver&& receiver,
                                   BoundArgs&&... bound_args) {
  return base::BindOnce(
      [](Method method, std::decay_t<Receiver> receiver,
         std::decay_t<BoundArgs>... bound_args, UnboundArgs... unbound_args) {
        (std::move(receiver).*method)(
            std::forward<BoundArgs>(bound_args)...,
            std::forward<UnboundArgs>(unbound_args)...);
      },
      method, std::move(receiver), std::forward<BoundArgs>(bound_args)...);
}

PrefetchServingHandle::OnIsolatedCookieCopyStartCallbackForTesting&
GetOnIsolatedCookieCopyStartCallbackForTesting() {
  static base::NoDestructor<
      PrefetchServingHandle::OnIsolatedCookieCopyStartCallbackForTesting>
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

PrefetchServingHandle::PrefetchServingHandle()
    : PrefetchServingHandle(nullptr, 0) {}

PrefetchServingHandle::PrefetchServingHandle(
    base::WeakPtr<PrefetchContainer> prefetch_container,
    size_t index_redirect_chain_to_serve)
    : prefetch_container_(std::move(prefetch_container)),
      index_redirect_chain_to_serve_(index_redirect_chain_to_serve) {}

PrefetchServingHandle::PrefetchServingHandle(PrefetchServingHandle&&) = default;
PrefetchServingHandle& PrefetchServingHandle::operator=(
    PrefetchServingHandle&&) = default;
PrefetchServingHandle::~PrefetchServingHandle() = default;

PrefetchServingHandle PrefetchServingHandle::Clone() {
  return PrefetchServingHandle(prefetch_container_,
                               index_redirect_chain_to_serve_);
}

const std::vector<std::unique_ptr<PrefetchSingleRedirectHop>>&
PrefetchServingHandle::redirect_chain() const {
  CHECK(GetPrefetchContainer());
  return GetPrefetchContainer()->redirect_chain(
      base::PassKey<PrefetchServingHandle>());
}

PrefetchNetworkContext* PrefetchServingHandle::GetCurrentNetworkContextToServe()
    const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToServe();
  return GetPrefetchContainer()->GetNetworkContext(
      this_prefetch.is_isolated_network_context_required_);
}

bool PrefetchServingHandle::HaveDefaultContextCookiesChanged() const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToServe();
  if (this_prefetch.cookie_listener_) {
    return this_prefetch.cookie_listener_->HaveCookiesChanged();
  }
  return false;
}

bool PrefetchServingHandle::HasIsolatedCookieCopyStarted() const {
  switch (GetCurrentSingleRedirectHopToServe().cookie_copy_status_) {
    case PrefetchSingleRedirectHop::CookieCopyStatus::kNotStarted:
      return false;
    case PrefetchSingleRedirectHop::CookieCopyStatus::kInProgress:
    case PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted:
      return true;
  }
}

bool PrefetchServingHandle::IsIsolatedCookieCopyInProgress() const {
  switch (GetCurrentSingleRedirectHopToServe().cookie_copy_status_) {
    case PrefetchSingleRedirectHop::CookieCopyStatus::kNotStarted:
    case PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted:
      return false;
    case PrefetchSingleRedirectHop::CookieCopyStatus::kInProgress:
      return true;
  }
}

void PrefetchServingHandle::
    SetOnIsolatedCookieCopyStartCallbackForTesting(  // IN-TEST
        PrefetchServingHandle::OnIsolatedCookieCopyStartCallbackForTesting
            on_isolated_cookie_copy_start_callback_for_testing) {
  GetOnIsolatedCookieCopyStartCallbackForTesting() =  // IN-TEST
      std::move(on_isolated_cookie_copy_start_callback_for_testing);
}

void PrefetchServingHandle::OnIsolatedCookieCopyStart() {
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
  GetPrefetchContainer()->PauseAllCookieListeners();

  GetCurrentSingleRedirectHopToServe().cookie_copy_status_ =
      PrefetchSingleRedirectHop::CookieCopyStatus::kInProgress;

  GetCurrentSingleRedirectHopToServe().cookie_copy_start_time_ =
      base::TimeTicks::Now();

  if (GetOnIsolatedCookieCopyStartCallbackForTesting()) {
    GetOnIsolatedCookieCopyStartCallbackForTesting().Run(*this);  // IN-TEST
  }
}

void PrefetchServingHandle::OnIsolatedCookiesReadCompleteAndWriteStart() {
  DCHECK(IsIsolatedCookieCopyInProgress());
  GetCurrentSingleRedirectHopToServe().cookie_read_end_and_write_start_time_ =
      base::TimeTicks::Now();
}

void PrefetchServingHandle::CopyIsolatedCookies() {
  DCHECK(IsValid());

  // We only need to copy cookies if the prefetch used an isolated network
  // context.
  if (!IsIsolatedNetworkContextRequiredToServe()) {
    return;
  }

  OnIsolatedCookieCopyStart();

  if (!GetCurrentNetworkContextToServe()) {
    CHECK_IS_TEST();
    // Not set in unit tests.
    return;
  }

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  GetCurrentNetworkContextToServe()->GetCookieManager()->GetCookieList(
      GetCurrentURLToServe(), options,
      net::CookiePartitionKeyCollection::Todo(),
      BindOnceForRvalueMemberMethod<const net::CookieAccessResultList&,
                                    const net::CookieAccessResultList&>(
          &PrefetchServingHandle::OnGotIsolatedCookiesForCopy, Clone()));
}

void PrefetchServingHandle::OnGotIsolatedCookiesForCopy(
    const net::CookieAccessResultList& cookie_list,
    const net::CookieAccessResultList& excluded_cookies) && {
  if (!IsValid()) {
    return;
  }

  OnIsolatedCookiesReadCompleteAndWriteStart();

  RecordPrefetchProxyPrefetchMainframeCookiesToCopy(cookie_list.size());

  if (cookie_list.empty()) {
    std::move(*this).OnIsolatedCookieCopyComplete();
    return;
  }

  const auto current_url = GetCurrentURLToServe();

  network::mojom::CookieManager* default_cookie_manager =
      GetPrefetchContainer()
          ->request()
          .browser_context()
          ->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();

  base::RepeatingClosure barrier = base::BarrierClosure(
      cookie_list.size(),
      BindOnceForRvalueMemberMethod(
          &PrefetchServingHandle::OnIsolatedCookieCopyComplete,
          std::move(*this)));

  // Do not touch `this` below, because `this` is already moved out here.

  net::CookieOptions options = net::CookieOptions::MakeAllInclusive();
  for (const net::CookieWithAccessResult& cookie : cookie_list) {
    default_cookie_manager->SetCanonicalCookie(
        cookie.cookie, current_url, options,
        base::BindOnce(
            [](base::RepeatingClosure closure,
               net::CookieAccessResult access_result) { closure.Run(); },
            barrier));
  }
}

void PrefetchServingHandle::OnIsolatedCookieCopyComplete() && {
  if (!IsValid()) {
    return;
  }

  DCHECK(IsIsolatedCookieCopyInProgress());

  // Resumes `PrefetchCookieListener` so that we can keep monitoring the
  // cookie change for the prefetch, which may be served again.
  GetPrefetchContainer()->ResumeAllCookieListeners();

  const auto& this_prefetch = GetCurrentSingleRedirectHopToServe();

  this_prefetch.cookie_copy_status_ =
      PrefetchSingleRedirectHop::CookieCopyStatus::kCompleted;

  if (this_prefetch.cookie_copy_start_time_.has_value() &&
      this_prefetch.cookie_read_end_and_write_start_time_.has_value()) {
    RecordCookieCopyTimes(
        this_prefetch.cookie_copy_start_time_.value(),
        this_prefetch.cookie_read_end_and_write_start_time_.value(),
        base::TimeTicks::Now());
  }

  if (this_prefetch.on_cookie_copy_complete_callback_) {
    std::move(this_prefetch.on_cookie_copy_complete_callback_).Run();
  }
}

// The `OnIsolatedCookie*ForTesting` methods are called different from the
// non-test code, e.g. `OnIsolatedCookieCopyCompleteForTesting()` is called on
// a cloned handle in the non-test code, but in the tests it is called on the
// original handle which is still used after this call. This can cause test-only
// inconsistencies but so far the tests are passing.
// TODO(crbug.com/480828677): Fix this.
void PrefetchServingHandle::OnIsolatedCookieCopyStartForTesting() {
  OnIsolatedCookieCopyStart();
}

void PrefetchServingHandle::
    OnIsolatedCookiesReadCompleteAndWriteStartForTesting() {
  OnIsolatedCookiesReadCompleteAndWriteStart();
}

void PrefetchServingHandle::OnIsolatedCookieCopyCompleteForTesting() {
  Clone().OnIsolatedCookieCopyComplete();
}

void PrefetchServingHandle::OnInterceptorCheckCookieCopy() {
  if (!GetCurrentSingleRedirectHopToServe().cookie_copy_start_time_) {
    return;
  }

  UMA_HISTOGRAM_CUSTOM_TIMES(
      "PrefetchProxy.AfterClick.Mainframe.CookieCopyStartToInterceptorCheck",
      base::TimeTicks::Now() -
          GetCurrentSingleRedirectHopToServe().cookie_copy_start_time_.value(),
      base::TimeDelta(), base::Seconds(5), 50);
}

void PrefetchServingHandle::SetOnCookieCopyCompleteCallback(
    base::OnceClosure callback) {
  DCHECK(IsIsolatedCookieCopyInProgress());

  GetCurrentSingleRedirectHopToServe().on_cookie_copy_complete_callback_ =
      std::move(callback);
}

std::pair<PrefetchRequestHandler, base::WeakPtr<ServiceWorkerClient>>
PrefetchServingHandle::CreateRequestHandler() {
  // Create a `PrefetchRequestHandler` from the current
  // `PrefetchSingleRedirectHop` and its corresponding
  // `PrefetchStreamingURLLoader`.
  auto handler = GetCurrentSingleRedirectHopToServe()
                     .response_reader_->CreateRequestHandler();

  // Advance the current `PrefetchSingleRedirectHop` position.
  AdvanceCurrentURLToServe();

  return handler;
}

bool PrefetchServingHandle::VariesOnCookieIndices() const {
  return GetCurrentSingleRedirectHopToServe()
      .response_reader_->VariesOnCookieIndices();
}

bool PrefetchServingHandle::MatchesCookieIndices(
    base::span<const std::pair<std::string, std::string>> cookies) const {
  return GetCurrentSingleRedirectHopToServe()
      .response_reader_->MatchesCookieIndices(cookies);
}

void PrefetchServingHandle::OnPrefetchProbeResult(
    PrefetchProbeResult probe_result) {
  GetPrefetchContainer()->SetProbeResult(base::PassKey<PrefetchServingHandle>(),
                                         probe_result);

  // TODO(https:crbug.com/433057364): Clean up the code below, so that we no
  // longer need `TriggeringOutcomeFromStatusForServingHandle`.

  // It's possible for the prefetch to fail (e.g., due to a network error) while
  // the origin probe is running. We avoid overwriting the status in that case.
  if (PrefetchContainer::TriggeringOutcomeFromStatusForServingHandle(
          base::PassKey<PrefetchServingHandle>(), GetPrefetchStatus()) ==
      PreloadingTriggeringOutcome::kFailure) {
    return;
  }

  switch (probe_result) {
    case PrefetchProbeResult::kNoProbing:
    case PrefetchProbeResult::kDNSProbeSuccess:
    case PrefetchProbeResult::kTLSProbeSuccess:
      // Wait to update the prefetch status until the probe for the final
      // redirect hop is a success.
      if (index_redirect_chain_to_serve_ == redirect_chain().size() - 1) {
        GetPrefetchContainer()->SetPrefetchStatus(
            PrefetchStatus::kPrefetchResponseUsed);
      }
      break;
    case PrefetchProbeResult::kDNSProbeFailure:
    case PrefetchProbeResult::kTLSProbeFailure:
      GetPrefetchContainer()->SetPrefetchStatus(
          PrefetchStatus::kPrefetchNotUsedProbeFailed);
      break;
    default:
      NOTIMPLEMENTED();
  }
}

bool PrefetchServingHandle::DoesCurrentURLToServeMatch(const GURL& url) const {
  CHECK(index_redirect_chain_to_serve_ >= 1);
  return GetCurrentSingleRedirectHopToServe().url_ == url;
}

bool PrefetchServingHandle::IsEnd() const {
  CHECK(index_redirect_chain_to_serve_ <= redirect_chain().size());
  return index_redirect_chain_to_serve_ >= redirect_chain().size();
}

const PrefetchSingleRedirectHop&
PrefetchServingHandle::GetCurrentSingleRedirectHopToServe() const {
  CHECK(index_redirect_chain_to_serve_ >= 0 &&
        index_redirect_chain_to_serve_ < redirect_chain().size());
  return *redirect_chain()[index_redirect_chain_to_serve_];
}

const GURL& PrefetchServingHandle::GetCurrentURLToServe() const {
  return GetCurrentSingleRedirectHopToServe().url_;
}

bool PrefetchServingHandle::IsIsolatedNetworkContextRequiredToServe() const {
  const PrefetchSingleRedirectHop& this_prefetch =
      GetCurrentSingleRedirectHopToServe();
  return this_prefetch.is_isolated_network_context_required_;
}

base::WeakPtr<PrefetchResponseReader>
PrefetchServingHandle::GetCurrentResponseReaderToServeForTesting() {
  return GetCurrentSingleRedirectHopToServe().response_reader_->GetWeakPtr();
}

PrefetchServableState PrefetchServingHandle::GetServableState() const {
  return GetPrefetchContainer()->GetServableState();
}

PrefetchServableState
PrefetchServingHandle::GetServableStateForTesting(  // IN-TEST
    base::TimeDelta cacheable_duration) const {
  return GetPrefetchContainer()->GetServableStateForTesting(  // IN-TEST
      cacheable_duration);
}

bool PrefetchServingHandle::HasPrefetchStatus() const {
  return GetPrefetchContainer()->HasPrefetchStatus();
}

PrefetchStatus PrefetchServingHandle::GetPrefetchStatus() const {
  return GetPrefetchContainer()->GetPrefetchStatus();
}

}  // namespace content
