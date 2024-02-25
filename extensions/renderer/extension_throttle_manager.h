// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_THROTTLE_MANAGER_H_
#define EXTENSIONS_RENDERER_EXTENSION_THROTTLE_MANAGER_H_

#include <map>
#include <memory>
#include <string>

#include "base/synchronization/lock.h"
#include "extensions/renderer/extension_throttle_entry.h"
#include "services/network/public/mojom/url_response_head.mojom-forward.h"
#include "url/gurl.h"

namespace blink {
class URLLoaderThrottle;
}  // namespace blink

namespace net {
struct RedirectInfo;
}  // namespace net

namespace network {
struct ResourceRequest;
}  // namespace network

namespace extensions {

// This is a thread safe class that registers URL request throttler entries for
// URLs being accessed in order to supervise traffic. URL requests for HTTP
// contents should register their URLs in this manager on each request.
//
// ExtensionThrottleManager maintains a map of URL IDs to URL request
// throttler entries. It creates URL request throttler entries when new URLs
// are registered, and does garbage collection from time to time in order to
// clean out outdated entries. URL ID consists of lowercased scheme, host, port
// and path. All URLs converted to the same ID will share the same entry.
class ExtensionThrottleManager {
 public:
  ExtensionThrottleManager();

  ExtensionThrottleManager(const ExtensionThrottleManager&) = delete;
  ExtensionThrottleManager& operator=(const ExtensionThrottleManager&) = delete;

  virtual ~ExtensionThrottleManager();

  // Creates a throttle which uses this class to prevent extensions from
  // requesting a URL too often, if such a throttle is needed.
  std::unique_ptr<blink::URLLoaderThrottle> MaybeCreateURLLoaderThrottle(
      const network::ResourceRequest& request);

  // Determine if a request to |request_url| should be rejected.
  bool ShouldRejectRequest(const GURL& request_url);

  // Determine if a redirect from the original |request_url| should be allowed
  // to be redirected as specified by |redirect_info|.
  bool ShouldRejectRedirect(const GURL& request_url,
                            const net::RedirectInfo& redirect_info);

  // Must be called when the |response_head| for a request has been received.
  void WillProcessResponse(
      const GURL& response_url,
      const network::mojom::URLResponseHead& response_head);

  // Set the network status online state as specified in |is_online|.
  void SetOnline(bool is_online);

  void SetBackoffPolicyForTests(
      std::unique_ptr<net::BackoffEntry::Policy> policy);

  // Registers a new entry in this service and overrides the existing entry (if
  // any) for the URL. The service will hold a reference to the entry.
  // It is only used by unit tests.
  void OverrideEntryForTests(const GURL& url,
                             std::unique_ptr<ExtensionThrottleEntry> entry);

  int GetNumberOfEntriesForTests() const { return url_entries_.size(); }

 protected:
  // Method that allows us to transform a URL into an ID that can be used in our
  // map. Resulting IDs will be lowercase and consist of the scheme, host, port
  // and path (without query string, fragment, etc.).
  // If the URL is invalid, the invalid spec will be returned, without any
  // transformation.
  std::string GetIdFromUrl(const GURL& url) const;

  // Must be called for every request, returns the URL request throttler entry
  // associated with the URL. The caller must inform this entry of some events.
  // Please refer to extension_throttle_entry.h for further information.
  ExtensionThrottleEntry* RegisterRequestUrl(const GURL& url);

  // Method that does the actual work of garbage collecting.
  void GarbageCollectEntries();

 private:
  // Explicitly erases an entry.
  // This is useful to remove those entries which have got infinite lifetime and
  // thus won't be garbage collected.
  // It is only used by unit tests.
  void EraseEntryForTests(const GURL& url);

  // Whether throttling is enabled or not.
  void set_enforce_throttling(bool enforce);
  bool enforce_throttling();

  // Method that ensures the map gets cleaned from time to time. The period at
  // which garbage collecting happens is adjustable with the
  // kRequestBetweenCollecting constant.
  void GarbageCollectEntriesIfNecessary();

  // Maximum number of entries that we are willing to collect in our map.
  static const unsigned int kMaximumNumberOfEntries;
  // Number of requests that will be made between garbage collection.
  static const unsigned int kRequestsBetweenCollecting;

  // Map that contains a list of URL ID (composed of the scheme, host, port and
  // path) and their matching ExtensionThrottleEntry.
  std::map<std::string, std::unique_ptr<ExtensionThrottleEntry>> url_entries_;

  // This keeps track of how many requests have been made. Used with
  // GarbageCollectEntries.
  unsigned int requests_since_last_gc_;

  // Valid after construction.
  GURL::Replacements url_id_replacements_;

  // This is null when it is not set for tests.
  std::unique_ptr<net::BackoffEntry::Policy> backoff_policy_for_tests_;

  // Used to synchronize all public methods.
  base::Lock lock_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_THROTTLE_MANAGER_H_
