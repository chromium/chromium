// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_INTEREST_GROUP_MANAGER_H_
#define CONTENT_PUBLIC_BROWSER_INTEREST_GROUP_MANAGER_H_

#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "content/common/content_export.h"
#include "url/origin.h"

namespace content {

// InterestGroupManager is a per-StoragePartition class that owns shared
// state needed to run FLEDGE auctions. It lives on the UI thread.
class InterestGroupManager {
 public:
  enum class TrustedServerAPIType {
    kInvalid,
    kBiddingAndAuction,
    kTrustedKeyValue,
    kMaxValue = kTrustedKeyValue,
  };

  struct InterestGroupDataKey {
    url::Origin owner;
    url::Origin joining_origin;

    bool operator<(const InterestGroupDataKey& other) const {
      return std::tie(owner, joining_origin) <
             std::tie(other.owner, other.joining_origin);
    }

    bool operator==(const InterestGroupDataKey& other) const {
      return std::tie(owner, joining_origin) ==
             std::tie(other.owner, other.joining_origin);
    }
  };

  // Gets a list of all interest group joining origins. Each joining origin
  // will only appear once. A joining origin is the top-frame origin for a page
  // on which the join action occurred. Interest groups the user is joined to
  // can be used later as part of on device ad auctions. Control over whether
  // sites can join clients to interest groups is provided by the
  // IsInterestGroupAPIAllowed function on the ContentBrowserClient.
  virtual void GetAllInterestGroupJoiningOrigins(
      base::OnceCallback<void(std::vector<url::Origin>)> callback) = 0;

  // Gets a list of all interest group data keys of owner origin and joining
  // origin pairs.
  virtual void GetAllInterestGroupDataKeys(
      base::OnceCallback<void(std::vector<InterestGroupDataKey>)> callback) = 0;

  // Removes all interest groups that match the given interest group data key of
  // same owner origin and joining origins.
  virtual void RemoveInterestGroupsByDataKey(InterestGroupDataKey data_key,
                                             base::OnceClosure callback) = 0;

  // Add a debugging override for a trused server's keys. The keys set will not
  // affect the state of on-disk databases, but will affect
  // `GetTrustedServerKey` while the current process is running. The callback
  // will be invoked with nullopt on success, an error string on failure.
  // Failures include being called when a configuration for `coordinator`
  // already exists, including from a previous code to this method.
  virtual void AddTrustedServerKeysDebugOverride(
      TrustedServerAPIType api,
      const url::Origin& coordinator,
      std::string serialized_keys,
      base::OnceCallback<void(std::optional<std::string>)> callback) = 0;

 protected:
  virtual ~InterestGroupManager() = default;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_INTEREST_GROUP_MANAGER_H_
