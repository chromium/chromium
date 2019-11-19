// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_throttler_manager.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "net/base/url_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source_type.h"

namespace net {

const unsigned int URLRequestThrottlerManager::kMaximumNumberOfEntries = 1500;
const unsigned int URLRequestThrottlerManager::kRequestsBetweenCollecting = 200;

URLRequestThrottlerManager::URLRequestThrottlerManager()
    : requests_since_last_gc_(0),
      logged_for_localhost_disabled_(false),
      registered_from_thread_(base::kInvalidThreadId) {
  url_id_replacements_.ClearPassword();
  url_id_replacements_.ClearUsername();
  url_id_replacements_.ClearQuery();
  url_id_replacements_.ClearRef();

  NetworkChangeNotifier::AddIPAddressObserver(this);
  NetworkChangeNotifier::AddConnectionTypeObserver(this);
}

URLRequestThrottlerManager::~URLRequestThrottlerManager() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NetworkChangeNotifier::RemoveIPAddressObserver(this);
  NetworkChangeNotifier::RemoveConnectionTypeObserver(this);

  // Since the manager object might conceivably go away before the
  // entries, detach the entries' back-pointer to the manager.
  auto i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if (i->second.get() != nullptr) {
      i->second->DetachManager();
    }
    ++i;
  }

  // Delete all entries.
  url_entries_.clear();
}

scoped_refptr<URLRequestThrottlerEntryInterface>
    URLRequestThrottlerManager::RegisterRequestUrl(const GURL &url) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  // Find the entry in the map or create a new NULL entry.
  scoped_refptr<URLRequestThrottlerEntry>& entry = url_entries_[url_id];

  // If the entry exists but could be garbage collected at this point, we
  // start with a fresh entry so that we possibly back off a bit less
  // aggressively (i.e. this resets the error count when the entry's URL
  // hasn't been requested in long enough).
  if (entry.get() && entry->IsEntryOutdated()) {
    entry = nullptr;
  }

  // Create the entry if needed.
  if (entry.get() == nullptr) {
    entry = new URLRequestThrottlerEntry(this, url_id);

    // We only disable back-off throttling on an entry that we have
    // just constructed.  This is to allow unit tests to explicitly override
    // the entry for localhost URLs.
    if (IsLocalhost(url)) {
      if (!logged_for_localhost_disabled_ && IsLocalhost(url)) {
        logged_for_localhost_disabled_ = true;
        net_log_.AddEventWithStringParams(
            NetLogEventType::THROTTLING_DISABLED_FOR_HOST, "host", url.host());
      }

      // TODO(joi): Once sliding window is separate from back-off throttling,
      // we can simply return a dummy implementation of
      // URLRequestThrottlerEntryInterface here that never blocks anything.
      entry->DisableBackoffThrottling();
    }
  }

  return entry;
}

void URLRequestThrottlerManager::OverrideEntryForTests(
    const GURL& url,
    URLRequestThrottlerEntry* entry) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);

  // Periodically garbage collect old entries.
  GarbageCollectEntriesIfNecessary();

  url_entries_[url_id] = entry;
}

void URLRequestThrottlerManager::EraseEntryForTests(const GURL& url) {
  // Normalize the url.
  std::string url_id = GetIdFromUrl(url);
  url_entries_.erase(url_id);
}

void URLRequestThrottlerManager::set_net_log(NetLog* net_log) {
  DCHECK(net_log);
  net_log_ = NetLogWithSource::Make(
      net_log, NetLogSourceType::EXPONENTIAL_BACKOFF_THROTTLING);
}

NetLog* URLRequestThrottlerManager::net_log() const {
  return net_log_.net_log();
}

void URLRequestThrottlerManager::OnIPAddressChanged() {
  OnNetworkChange();
}

void URLRequestThrottlerManager::OnConnectionTypeChanged(
    NetworkChangeNotifier::ConnectionType type) {
  OnNetworkChange();
}

std::string URLRequestThrottlerManager::GetIdFromUrl(const GURL& url) const {
  if (!url.is_valid())
    return url.possibly_invalid_spec();

  GURL id = url.ReplaceComponents(url_id_replacements_);
  return base::ToLowerASCII(id.spec());
}

void URLRequestThrottlerManager::GarbageCollectEntriesIfNecessary() {
  requests_since_last_gc_++;
  if (requests_since_last_gc_ < kRequestsBetweenCollecting)
    return;
  requests_since_last_gc_ = 0;

  GarbageCollectEntries();
}

void URLRequestThrottlerManager::GarbageCollectEntries() {
  auto i = url_entries_.begin();
  while (i != url_entries_.end()) {
    if ((i->second)->IsEntryOutdated()) {
      url_entries_.erase(i++);
    } else {
      ++i;
    }
  }

  // In case something broke we want to make sure not to grow indefinitely.
  while (url_entries_.size() > kMaximumNumberOfEntries) {
    url_entries_.erase(url_entries_.begin());
  }
}

void URLRequestThrottlerManager::OnNetworkChange() {
  // Remove all entries.  Any entries that in-flight requests have a reference
  // to will live until those requests end, and these entries may be
  // inconsistent with new entries for the same URLs, but since what we
  // want is a clean slate for the new connection type, this is OK.
  url_entries_.clear();
  requests_since_last_gc_ = 0;
}

}  // namespace net
