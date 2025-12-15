// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/loader/cookie_jar.h"

#include <cstdint>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "mojo/public/cpp/base/shared_memory_version.h"
#include "services/network/public/mojom/restricted_cookie_manager.mojom-forward.h"
#include "third_party/blink/public/platform/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/web_feature.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/kurl_hash.h"
#include "third_party/blink/renderer/platform/wtf/hash_functions.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {
namespace {

enum class CookieCacheLookupResult {
  kCacheMissFirstAccess = 0,
  kCacheHitAfterGet = 1,
  kCacheHitAfterSet = 2,
  kCacheMissAfterGet = 3,
  kCacheMissAfterSet = 4,
  kMaxValue = kCacheMissAfterSet,
};

// Histogram for tracking first cookie requests.
constexpr char kFirstCookieRequestHistogram[] =
    "Blink.Experimental.Cookies.FirstCookieRequest";

}  // namespace

// Controls whether we apply an artificial delay to priming the CookieJar access
// for all APIs. There are 2 parameters for each API that influence how long the
// delay is, `factor` and `offset`. If the actual time taken is `elapsed` then
// the delay will be `elapsed * factor + offset`.
BASE_FEATURE(kCookieJarAblation, base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(double,
                   kCookieJarAblationDelayFactor,
                   &kCookieJarAblation,
                   "factor",
                   0.0);
BASE_FEATURE_PARAM(base::TimeDelta,
                   kCookieJarAblationDelayOffset,
                   &kCookieJarAblation,
                   "offset",
                   base::Milliseconds(0));

CookieJar::CookieJar(blink::Document* document)
    : backend_(document->GetExecutionContext()), document_(document) {}

CookieJar::~CookieJar() = default;

void CookieJar::Trace(Visitor* visitor) const {
  visitor->Trace(backend_);
  visitor->Trace(document_);
}

void CookieJar::SetCookie(const String& value) {
  TRACE_EVENT("blink", "CookieJar::SetCookie");
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return;

  base::ElapsedTimer timer;
  RequestRestrictedCookieManagerIfNeeded();
  bool is_ad_tagged =
      document_->GetFrame() && document_->GetFrame()->IsAdFrame();

  CookiesResponsePtr response;
  const bool get_version_shared_memory =
      !shared_memory_version_client_.has_value();
  const bool apply_devtools_overrides = ShouldApplyDevtoolsOverrides();
  if (RuntimeEnabledFeatures::AsyncSetCookieEnabled()) {
    required_committed_writes_++;
    backend_->SetCookieFromString(
        cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
        document_->GetExecutionContext()->GetStorageAccessApiStatus(),
        get_version_shared_memory, is_ad_tagged, apply_devtools_overrides,
        value,
        BindOnce(&CookieJar::OnSetCookieResponse, WrapWeakPersistent(this),
                 cookie_url, apply_devtools_overrides));
  } else {
    if (!backend_->SetCookieFromString(
            cookie_url, document_->SiteForCookies(),
            document_->TopFrameOrigin(),
            document_->GetExecutionContext()->GetStorageAccessApiStatus(),
            get_version_shared_memory, is_ad_tagged, apply_devtools_overrides,
            value, &response)) {
      // On IPC failure invalidate cached values and return empty string since
      // there is no guarantee the client can still validly access cookies in
      // the current context. See crbug.com/1468909.
      InvalidateCache();
      return;
    }
    OnSetCookieResponse(cookie_url, apply_devtools_overrides,
                        std::move(response));
  }
  last_operation_was_set_ = true;

  base::TimeDelta elapsed = timer.Elapsed();
  base::UmaHistogramTimes("Blink.SetCookieTime", elapsed);

  if (base::FeatureList::IsEnabled(kCookieJarAblation)) {
    base::TimeDelta delay = elapsed * kCookieJarAblationDelayFactor.Get() +
                            kCookieJarAblationDelayOffset.Get();
    base::UmaHistogramMediumTimes("Blink.SetCookieTime.AblationDelay", delay);
    if (delay.is_positive()) {
      base::PlatformThread::Sleep(delay);
    }
  }

  if (is_first_operation_) {
    LogFirstCookieRequest(FirstCookieRequest::kFirstOperationWasSet);
  }
}

void CookieJar::OnSetCookieResponse(const KURL& cookie_url,
                                    bool apply_devtools_overrides,
                                    CookiesResponsePtr response) {
  if (response) {
    if (response->version_buffer.IsValid()) {
      shared_memory_version_client_.emplace(
          std::move(response->version_buffer));
    }

    // When features GetCookiesOnSet is disabled, an invalid version is
    // returned, then don't update the cache.
    if (response->version != mojo::shared_memory_version::kInvalidVersion &&
        response->version > last_version_) {
      last_devtools_overrides_were_applied = apply_devtools_overrides;
      last_cookies_ = response->cookies;
      UpdateCacheAfterGetRequest(cookie_url, response->cookies,
                                 response->version);
    }
  }
}

void CookieJar::OnBackendDisconnect() {
  shared_memory_version_client_.reset();
  InvalidateCache();
}

String CookieJar::Cookies() {
  TRACE_EVENT("blink", "CookieJar::Cookies");
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return String();

  base::ElapsedTimer timer;

  // This can affect the result of the IPCNeeded() call below, so needs to be
  // done first.
  const RequestCookieManagerPipeState pipe_state =
      RequestRestrictedCookieManagerIfNeeded();

  String value = g_empty_string;

  // Store the latest cookie version to update |last_version_| after attempting
  // to get the string. Will get updated once more by GetCookiesString() if an
  // ipc is required.
  uint64_t new_version = last_version_;
  bool should_apply_devtools_overrides = ShouldApplyDevtoolsOverrides();
  const bool ipc_needed = IPCNeeded(should_apply_devtools_overrides);
  base::UmaHistogramBoolean("Blink.Experimental.Cookies.IpcNeeded", ipc_needed);
  if (ipc_needed) {
    base::ReadOnlySharedMemoryRegion new_mapped_region;
    const bool get_version_shared_memory =
        !shared_memory_version_client_.has_value();

    bool is_ad_tagged =
        document_->GetFrame() && document_->GetFrame()->IsAdFrame();

    if (!backend_->GetCookiesString(
            cookie_url, document_->SiteForCookies(),
            document_->TopFrameOrigin(),
            document_->GetExecutionContext()->GetStorageAccessApiStatus(),
            get_version_shared_memory, is_ad_tagged,
            should_apply_devtools_overrides,
            /*force_disable_third_party_cookies=*/false, &new_version,
            &new_mapped_region, &value)) {
      // On IPC failure invalidate cached values and return empty string since
      // there is no guarantee the client can still validly access cookies in
      // the current context. See crbug.com/1468909.
      InvalidateCache();
      return g_empty_string;
    }
    last_cookies_ = value;
    if (new_mapped_region.IsValid()) {
      shared_memory_version_client_.emplace(std::move(new_mapped_region));
    }
  }

  base::TimeDelta elapsed = timer.Elapsed();
  constexpr int kMinTimeMicros = 10;
  constexpr int kMaxTimeMicros = 1 * 1000 * 1000;  // 1 second
  if (ipc_needed) {
    base::UmaHistogramCustomCounts("Blink.CookiesTime.IpcNeeded2",
                                   elapsed.InMicroseconds(), kMinTimeMicros,
                                   kMaxTimeMicros, 50);

    // Temporary histograms to investigate https://crbug.com/414748254.
    switch (pipe_state) {
      case RequestCookieManagerPipeState::kNoOldPipe:
        base::UmaHistogramTimes("Blink.CookiesTime.NoOldPipe", elapsed);
        break;
      case RequestCookieManagerPipeState::kDisconnectedOldPipe:
        base::UmaHistogramTimes("Blink.CookiesTime.DisconnectedOldPipe",
                                elapsed);
        break;
      case RequestCookieManagerPipeState::kConnectedOldPipe:
        base::UmaHistogramTimes("Blink.CookiesTime.ConnectedOldPipe", elapsed);
        break;
    }
  } else {
    base::UmaHistogramCustomCounts("Blink.CookiesTime.IpcNotNeeded2",
                                   elapsed.InMicroseconds(), kMinTimeMicros,
                                   kMaxTimeMicros, 50);
  }

  // We should run the ablation study only for scenarios with ipc.
  if (base::FeatureList::IsEnabled(kCookieJarAblation) && ipc_needed) {
    base::TimeDelta delay = elapsed * kCookieJarAblationDelayFactor.Get() +
                            kCookieJarAblationDelayOffset.Get();
    base::UmaHistogramMediumTimes("Blink.CookiesTime.AblationDelay2", delay);

    if (delay.is_positive()) {
      // Report the actual delay caused by PlatformThread::Sleep(). See
      // https://crbug.com/412532502 for more details.
      SCOPED_UMA_HISTOGRAM_TIMER_MICROS("Blink.CookiesTime.AblationSleepTime");
      base::PlatformThread::Sleep(delay);
    }
  }

  UpdateCacheAfterGetRequest(cookie_url, value, new_version);

  last_operation_was_set_ = false;
  if (is_first_operation_) {
    LogFirstCookieRequest(FirstCookieRequest::kFirstOperationWasGet);
  }
  return last_cookies_;
}

bool CookieJar::CookiesEnabled() {
  KURL cookie_url = document_->CookieURL();
  if (cookie_url.IsEmpty())
    return false;

  base::ElapsedTimer timer;
  RequestRestrictedCookieManagerIfNeeded();
  bool cookies_enabled = false;
  backend_->CookiesEnabledFor(
      cookie_url, document_->SiteForCookies(), document_->TopFrameOrigin(),
      document_->GetExecutionContext()->GetStorageAccessApiStatus(),
      ShouldApplyDevtoolsOverrides(), &cookies_enabled);
  base::UmaHistogramTimes("Blink.CookiesEnabledTime", timer.Elapsed());
  if (is_first_operation_) {
    LogFirstCookieRequest(FirstCookieRequest::kFirstOperationWasCookiesEnabled);
  }
  return cookies_enabled;
}

void CookieJar::SetCookieManager(
    mojo::PendingRemote<network::mojom::blink::RestrictedCookieManager>
        cookie_manager) {
  backend_.reset();
  backend_.Bind(std::move(cookie_manager),
                document_->GetTaskRunner(TaskType::kInternalDefault));
}

void CookieJar::InvalidateCache() {
  last_cookies_hash_.reset();
  last_cookies_ = String();
  last_version_ = mojo::shared_memory_version::kInvalidVersion;
}

bool CookieJar::IPCNeeded(bool should_apply_devtools_overrides) {
  // IPC needed if devtools overrides is different
  if (should_apply_devtools_overrides != last_devtools_overrides_were_applied) {
    last_devtools_overrides_were_applied = should_apply_devtools_overrides;
    return true;
  }

  // |last_cookies_| can be null when converting the raw mojo payload failed.
  // (See ConvertUTF8ToUTF16() for details.) In that case use an IPC to request
  // another string to be safe.
  if (last_cookies_.IsNull()) {
    return true;
  }

  // No shared memory communication so IPC needed.
  if (!shared_memory_version_client_.has_value()) {
    return true;
  }

  // Pending write commits.
  // When AsyncSetCookie is disabled, required_committed_writes_ always equals
  // 0, then this check is never true.
  if (shared_memory_version_client_->CommittedWritesIsLessThan(
          required_committed_writes_)) {
    return true;
  }

  // Cookie string has changed.
  if (shared_memory_version_client_->SharedVersionIsGreaterThan(
          last_version_)) {
    return true;
  }

  // No IPC needed!
  return false;
}

CookieJar::RequestCookieManagerPipeState
CookieJar::RequestRestrictedCookieManagerIfNeeded() {
  RequestCookieManagerPipeState pipe_state =
      RequestCookieManagerPipeState::kConnectedOldPipe;
  if (!backend_.is_bound() || !backend_.is_connected()) {
    if (!backend_.is_bound()) {
      pipe_state = RequestCookieManagerPipeState::kNoOldPipe;
    } else {
      pipe_state = RequestCookieManagerPipeState::kDisconnectedOldPipe;
    }
    backend_.reset();

    // Either the backend was never bound or it became unbound. In case we're in
    // the unbound case perform the appropriate cleanup.
    OnBackendDisconnect();

    document_->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
        backend_.BindNewPipeAndPassReceiver(
            document_->GetTaskRunner(TaskType::kInternalDefault)));
  }
  return pipe_state;
}

void CookieJar::UpdateCacheAfterGetRequest(const KURL& cookie_url,
                                           const String& cookie_string,
                                           uint64_t new_version) {
  std::optional<unsigned> new_hash =
      HashInts(blink::GetHash(cookie_url),
               cookie_string.IsNull() ? 0 : blink::GetHash(cookie_string));

  CookieCacheLookupResult result =
      CookieCacheLookupResult::kCacheMissFirstAccess;

  // An invalid version means no shared memory communication so assume changes
  // happened.
  const bool cookie_is_unchanged =
      new_version != mojo::shared_memory_version::kInvalidVersion &&
      last_version_ == new_version;

  if (last_cookies_hash_.has_value() && cookie_is_unchanged) {
    if (last_cookies_hash_ == new_hash) {
      result = last_operation_was_set_
                   ? CookieCacheLookupResult::kCacheHitAfterSet
                   : CookieCacheLookupResult::kCacheHitAfterGet;
    } else {
      result = last_operation_was_set_
                   ? CookieCacheLookupResult::kCacheMissAfterSet
                   : CookieCacheLookupResult::kCacheMissAfterGet;
    }
  }

  UMA_HISTOGRAM_ENUMERATION("Blink.Experimental.Cookies.CacheLookupResult2",
                            result);

  // Update the version to what it was before getting the string, ignoring any
  // changes that could have happened since then. This ensures as "stale" a
  // version as possible is used. This is the desired effect to avoid inhibiting
  // IPCs when not desired.
  last_version_ = new_version;
  last_cookies_hash_ = new_hash;
}

void CookieJar::LogFirstCookieRequest(FirstCookieRequest first_cookie_request) {
  is_first_operation_ = false;
  base::UmaHistogramEnumeration(kFirstCookieRequestHistogram,
                                first_cookie_request);
}

bool CookieJar::ShouldApplyDevtoolsOverrides() const {
  bool should_apply_devtools_overrides = false;
  probe::ShouldApplyDevtoolsCookieSettingOverrides(
      document_->GetExecutionContext(), &should_apply_devtools_overrides);

  return should_apply_devtools_overrides;
}

}  // namespace blink
