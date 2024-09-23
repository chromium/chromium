// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extension_throttle_manager.h"

#include <map>
#include <utility>

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "base/strings/string_util.h"
#include "extensions/common/constants.h"
#include "extensions/renderer/extension_url_loader_throttle.h"
#include "net/base/url_util.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/platform/web_url.h"

namespace extensions {

const unsigned int ExtensionThrottleManager::kMaximumNumberOfEntries = 1500;
const unsigned int ExtensionThrottleManager::kRequestsBetweenCollecting = 200;

ExtensionThrottleManager::ExtensionThrottleManager()
    : requests_since_last_gc_(0) {
  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();
}

ExtensionThrottleManager::~ExtensionThrottleManager() {
  base::AutoLock auto_lock(lock_);
  // Delete all entries.
  url_entries_.clear();
}

std::unique_ptr<blink::URLLoaderThrottle>
ExtensionThrottleManager::MaybeCreateURLLoaderThrottle(
    const network::ResourceRequest& request) {
  // TODO(crbug.com/40113701): This relies on the extension scheme
  // getting special handling via ShouldTreatURLSchemeAsFirstPartyWhenTopLevel,
  // which has problems. Once that's removed this should probably look at top
  // level directly instead.
  if (request.site_for_cookies.scheme() != extensions::kExtensionScheme) {
    return nullptr;
  }
  return std::make_unique<ExtensionURLLoaderThrottle>(this);
}

ExtensionThrottleEntry* ExtensionThrottleManager::RegisterRequestUrl(
    const GURL& url) {
  // Internal function, no locking.

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create a new null entry.
  std::unique_ptr<ExtensionThrottleEntry>& entry = url_entries_[url_id];

  // If the entry exists but could be garbage collected at this point, we
  // start with a fresh entry so that we possibly back off a bit less
  // aggressively (i.e. this resets the error count when the entry's URL
  // hasn't been requested in long enough).
  if (entry && entry->IsEntryOutdated())
    entry.reset();

  // Create the entry if needed.
  if (!entry) {
    if (backoff_policy_for_tests_) {
      entry = std::make_unique<ExtensionThrottleEntry>(
          url_id, backoff_policy_for_tests_.get());
    } else {
      entry = std::make_unique<ExtensionThrottleEntry>(url_id);
    }

    // We only disable back-off throttling on an entry that we have
    // just constructed.  This is to allow unit tests to explicitly override
    // the entry for localhost URLs.
    if (net::IsLocalhost(url)) {
      // TODO(joi): Once sliding window is separate from back-off throttling,
      // we can simply return a dummy implementation of
      // ExtensionThrottleEntry here that never blocks anything.
      entry->DisableBackoffThrottling();
    }
  }

  return entry.get();
}

bool ExtensionThrottleManager::ShouldRejectRequest(const GURL& request_url) {
  base::AutoLock auto_lock(lock_);
  return RegisterRequestUrl(request_url)->ShouldRejectRequest();
}

bool ExtensionThrottleManager::ShouldRejectRedirect(
    const GURL& request_url,
    const net::RedirectInfo& redirect_info) {
  {
    // An entry GC when requests are outstanding can purge entries so check
    // before use.
    base::AutoLock auto_lock(lock_);
    auto it = url_entries_.find(GetIdFromUrl(request_url));
    if (it != url_entries_.end())
      it->second->UpdateWithResponse(redirect_info.status_code);
  }
  return ShouldRejectRequest(redirect_info.new_url);
}

void ExtensionThrottleManager::WillProcessResponse(
    const GURL& response_url,
    const network::mojom::URLResponseHead& response_head) {
  if (response_head.network_accessed) {
    // An entry GC when requests are outstanding can purge entries so check
    // before use.
    base::AutoLock auto_lock(lock_);
    auto it = url_entries_.find(GetIdFromUrl(response_url));
    if (it != url_entries_.end())
      it->second->UpdateWithResponse(response_head.headers->response_code());
  }
}

void ExtensionThrottleManager::SetBackoffPolicyForTests(
    std::unique_ptr<net::BackoffEntry::Policy> policy) {
  base::AutoLock auto_lock(lock_);
  backoff_policy_for_tests_ = std::move(policy);
}

void ExtensionThrottleManager::OverrideEntryForTests(
    const GURL& url,
    std::unique_ptr<ExtensionThrottleEntry> entry) {
  base::AutoLock auto_lock(lock_);
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  url_entries_[url_id] = std::move(entry);
}

void ExtensionThrottleManager::EraseEntryForTests(const GURL& url) {
  base::AutoLock auto_lock(lock_);
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);
  url_entries_.erase(url_id);
}

void ExtensionThrottleManager::SetOnline(bool is_online) {
  // When we switch from online to offline or change IP addresses, we
  // clear all back-off history. This is a precaution in case the change in
  // online state now lets us communicate without error with servers that
  // we were previously getting 500 or 503 responses from (perhaps the
  // responses are from a badly-written proxy that should have returned a
  // 502 or 504 because it's upstream connection was down or it had no route
  // to the server).
  // Remove all entries.  Any entries that in-flight requests have a reference
  // to will live until those requests end, and these entries may be
  // inconsistent with new entries for the same URLs, but since what we
  // want is a clean slate for the new connection type, this is OK.
  base::AutoLock auto_lock(lock_);
  url_entries_.clear();
  requests_since_last_gc_ = 0;
}

std::string ExtensionThrottleManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  GURL id = url.ReplaceComponents(url_id_replacements_);
  return base::ToLowerASCII(id.spec());
}

void ExtensionThrottleManager::GarbageCollectEntriesIfNecessary() {
  requests_since_last_gc_++;
  if (requests_since_last_gc_ < kRequestsBetweenCollecting)
    return;
  requests_since_last_gc_ = 0;

  GarbageCollectEntries();
}

void ExtensionThrottleManager::GarbageCollectEntries() {
  std::erase_if(url_entries_, [](const auto& entry) {
    return entry.second->IsEntryOutdated();
  });

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

}  // namespace extensions
