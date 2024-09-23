// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PERMISSIONS_CHECKER_H_
#define CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PERMISSIONS_CHECKER_H_

#include "content/common/content_export.h"

#include <list>
#include <map>
#include <memory>
#include <tuple>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "content/browser/interest_group/interest_group_permissions_cache.h"
#include "net/base/network_isolation_key.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/origin.h"

namespace network {
class SimpleURLLoader;
}

namespace content {

// Responsible for checking whether a frame can join or leave an interest group.
// Performs .well-known fetches if necessary.
//
// Groups permissions checks that can be handled by the same .well-known fetch,
// to avoid redundant fetches.
//
// TODO(crbug.com/40221941):
// * Consider adding a per-NIK / per-page / per-frame LRU cache. Roundtrip to
//   the HTTP cache are slow, and we'll likely want to limit the number of
//   pending operations the renderer sends to the browser process at a time.
// * Add detailed error information to DevTools or as a Promise failure on
//   rejection.
// * Figure out integration with IsInterestGroupAPIAllowed() - e.g., for
//   cross-origin iframes, there are 3 origins (top-level frame, iframe,
//   interest group frame). Currently we're not considering iframe origin at
//   all. Should we be?
class CONTENT_EXPORT InterestGroupPermissionsChecker {
 public:
  // More than enough space, since current spec is a dictionary of at most two
  // entries, and each entry has keys and values from a fixed set, which have
  // limited lengths.
  static constexpr int kMaxBodySize = 8192;

  static const base::TimeDelta kRequestTimeout;

  enum class Operation {
    kJoin,
    kLeave,
  };

  using PermissionsCheckCallback =
      base::OnceCallback<void(bool operation_allowed)>;

  InterestGroupPermissionsChecker();

  InterestGroupPermissionsChecker(InterestGroupPermissionsChecker&) = delete;
  InterestGroupPermissionsChecker& operator=(InterestGroupPermissionsChecker&) =
      delete;
  ~InterestGroupPermissionsChecker();

  // Checks if `frame_origin` can perform `operation` on an interest group owned
  // by `interest_group_owner`. Fetches a .well-known URL using
  // `url_loader_factory` if needed to check permissions. Has no rate limiting,
  // so a fetch will always be started immediately, if one is needed.
  //
  // Invokes `permissions_check_callback` with the result of the check. May
  // invoke callback synchronously, in cases where no .well-known fetch is
  // needed.
  //
  // `permissions_check_callback` may not call CheckPermissions() or delete
  // `this` when invoked.
  //
  // Performs no validity checks on origins. It's up to the caller to make sure
  // they're HTTPS, and provided origins are in general allowed to join/leave
  // interest groups.
  void CheckPermissions(Operation operation,
                        const url::Origin& frame_origin,
                        const url::Origin& interest_group_owner,
                        const net::NetworkIsolationKey& network_isolation_key,
                        network::mojom::URLLoaderFactory& url_loader_factory,
                        PermissionsCheckCallback permissions_check_callback);

  void ClearCache();

  InterestGroupPermissionsCache& cache_for_testing() { return cache_; }

 private:
  using Permissions = InterestGroupPermissionsCache::Permissions;

  // Two permissions checks with the same key can use the same .well-known
  // response, though they may have a different associated Operation.
  struct PermissionsKey {
    bool operator<(const PermissionsKey& other) const {
      return std::tie(frame_origin, interest_group_owner,
                      network_isolation_key) <
             std::tie(other.frame_origin, other.interest_group_owner,
                      other.network_isolation_key);
    }

    url::Origin frame_origin;
    url::Origin interest_group_owner;
    net::NetworkIsolationKey network_isolation_key;
  };

  // A permissions check waiting on a .well-known fetch to complete.
  struct PendingPermissionsCheck {
    PendingPermissionsCheck(
        Operation operation,
        PermissionsCheckCallback permissions_check_callback);
    PendingPermissionsCheck(PendingPermissionsCheck&&);
    ~PendingPermissionsCheck();

    Operation operation;
    PermissionsCheckCallback permissions_check_callback;
  };

  // A request for a .well-known file, along with a list of
  // PendingPermissionChecks that are waiting on it to complete. Deleted once
  // `simple_url_loader` has received a response, and its JSON has been parsed
  // by an async DataDecoder::ParseJsonIsolated() call.
  struct ActiveRequest {
    ActiveRequest();
    ~ActiveRequest();

    std::list<PendingPermissionsCheck> pending_checks;

    // Used to fetch the .well-known URL.
    std::unique_ptr<network::SimpleURLLoader> simple_url_loader;
  };

  // A map of interest group origins to their ActiveRequests.
  using ActiveRequestMap =
      std::map<PermissionsKey, std::unique_ptr<ActiveRequest>>;

  // Invoked with the result of the ".well-known" fetch for `active_request`.
  void OnRequestComplete(ActiveRequestMap::iterator active_request,
                         std::unique_ptr<std::string> response_body);

  // Invoked with the result of parsing the response body associated with
  // `active_request` as JSON.
  void OnJsonParsed(ActiveRequestMap::iterator active_request,
                    data_decoder::DataDecoder::ValueOrError result);

  // Invoked once the Permissions to use for `active_request` have been
  // determined, either as result of an error, by successfully parsing the
  // result of a .well-known request as JSON.
  void OnActiveRequestComplete(ActiveRequestMap::iterator active_request,
                               Permissions permissions);

  // Returns true if `permissions` allows `operation`.
  static bool AllowsOperation(Permissions permissions, Operation operation);

  ActiveRequestMap active_requests_;
  InterestGroupPermissionsCache cache_;

  base::WeakPtrFactory<InterestGroupPermissionsChecker> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_INTEREST_GROUP_PERMISSIONS_CHECKER_H_
