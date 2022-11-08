// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_
#define NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_

#include <map>
#include <set>
#include <string>

#include "base/memory/scoped_refptr.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_checker.h"
#include "net/base/net_export.h"
#include "net/base/network_change_notifier.h"
#include "net/url_request/url_request_throttler_entry.h"
#include "url/gurl.h"

namespace net {

class NetLog;
class NetLogWithSource;

// Class that registers URL request throttler entries for URLs being accessed
// in order to supervise traffic. URL requests for HTTP contents should
// register their URLs in this manager on each request.
//
// URLRequestThrottlerManager maintains a map of URL IDs to URL request
// throttler entries. It creates URL request throttler entries when new URLs
// are registered, and does garbage collection from time to time in order to
// clean out outdated entries. URL ID consists of lowercased scheme, host, port
// and path. All URLs converted to the same ID will share the same entry.
class NET_EXPORT_PRIVATE URLRequestThrottlerManager
    : public NetworkChangeNotifier::IPAddressObserver,
      public NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  URLRequestThrottlerManager();

  URLRequestThrottlerManager(const URLRequestThrottlerManager&) = delete;
  URLRequestThrottlerManager& operator=(const URLRequestThrottlerManager&) =
      delete;

  ~URLRequestThrottlerManager() override;

  // Must be called for every request, returns the URL request throttler entry
  // associated with the URL. The caller must inform this entry of some events.
  // Please refer to url_request_throttler_entry_interface.h for further
  // informations.
  scoped_refptr<URLRequestThrottlerEntryInterface> RegisterRequestUrl(
      const GURL& url);

  // Registers a new entry in this service and overrides the existing entry (if
  // any) for the URL. The service will hold a reference to the entry.
  // It is only used by unit tests.
  void OverrideEntryForTests(const GURL& url,
                             scoped_refptr<URLRequestThrottlerEntry> entry);

  // Explicitly erases an entry.
  // This is useful to remove those entries which have got infinite lifetime and
  // thus won't be garbage collected.
  // It is only used by unit tests.
  void EraseEntryForTests(const GURL& url);

  // Whether throttling is enabled or not.
  void set_enforce_throttling(bool enforce);
  bool enforce_throttling();

  // Sets the NetLog instance to use.
  void set_net_log(NetLog* net_log);
  NetLog* net_log() const;

  // IPAddressObserver interface.
  void OnIPAddressChanged() override;

  // ConnectionTypeObserver interface.
  void OnConnectionTypeChanged(
      NetworkChangeNotifier::ConnectionType type) override;

  // Method that allows us to transform a URL into an ID that can be used in our
  // map. Resulting IDs will be lowercase and consist of the scheme, host, port
  // and path (without query string, fragment, etc.).
  // If the URL is invalid, the invalid spec will be returned, without any
  // transformation.
  std::string GetIdFromUrl(const GURL& url) const;

  // Method that ensures the map gets cleaned from time to time. The period at
  // which garbage collecting happens is adjustable with the
  // kRequestBetweenCollecting constant.
  void GarbageCollectEntriesIfNecessary();

  // Method that does the actual work of garbage collecting.
  void GarbageCollectEntries();

  // When we switch from online to offline or change IP addresses, we
  // clear all back-off history. This is a precaution in case the change in
  // online state now lets us communicate without error with servers that
  // we were previously getting 500 or 503 responses from (perhaps the
  // responses are from a badly-written proxy that should have returned a
  // 502 or 504 because it's upstream connection was down or it had no route
  // to the server).
  void OnNetworkChange();

  // Used by tests.
  int GetNumberOfEntriesForTests() const {
    return static_cast<int>(url_entries_.size());
  }

 private:
  // From each URL we generate an ID composed of the scheme, host, port and path
  // that allows us to uniquely map an entry to it.
  typedef std::map<std::string, scoped_refptr<URLRequestThrottlerEntry> >
      UrlEntryMap;

  // Maximum number of entries that we are willing to collect in our map.
  static const unsigned int kMaximumNumberOfEntries;
  // Number of requests that will be made between garbage collection.
  static const unsigned int kRequestsBetweenCollecting;

  // Map that contains a list of URL ID and their matching
  // URLRequestThrottlerEntry.
  UrlEntryMap url_entries_;

  // This keeps track of how many requests have been made. Used with
  // GarbageCollectEntries.
  unsigned int requests_since_last_gc_ = 0;

  // Valid after construction.
  GURL::Replacements url_id_replacements_;

  // Certain tests do not obey the net component's threading policy, so we
  // keep track of whether we're being used by tests, and turn off certain
  // checks.
  //
  // TODO(joi): See if we can fix the offending unit tests and remove this
  // workaround.
  bool enable_thread_checks_;

  // Initially false, switches to true once we have logged because of back-off
  // being disabled for localhost.
  bool logged_for_localhost_disabled_ = false;

  // NetLog to use, if configured.
  NetLogWithSource net_log_;

  // Valid once we've registered for network notifications.
  base::PlatformThreadId registered_from_thread_ = base::kInvalidThreadId;

  THREAD_CHECKER(thread_checker_);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_THROTTLER_MANAGER_H_
