// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_K_ANONYMITY_SERVICE_DELEGATE_H_
#define CONTENT_PUBLIC_BROWSER_K_ANONYMITY_SERVICE_DELEGATE_H_

#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// The KAnonymityServiceDelegate interface provides functions to determine if
// the provided ID is k-anonymous in the sense that QuerySet only returns true
// for an ID if at least k other users have called JoinSet for that ID within
// an implementation defined time window. The value k is defined the by the
// implementer of this interface.
class CONTENT_EXPORT KAnonymityServiceDelegate {
 public:
  virtual ~KAnonymityServiceDelegate() = default;

  // JoinSets marks the provided ID as being used by this client. The
  // caller of this function should avoid calling this function repeatedly for
  // the same IDs, as there is no guarantee the implementer deduplicates calls.
  // The callback is called asynchronously when the join has completed or
  // failed. A value of 'true' passed to the callback indicates the join
  // completed successfully. The ID is already hashed via SHA256.
  virtual void JoinSet(std::string id,
                       base::OnceCallback<void(bool)> callback) = 0;

  // QuerySet requests the k-anonymity status of the provided IDs. The callback
  // will be called asynchronously. If the request is successful, the callback
  // will be passed a vector where `true` means the ID in the same index of the
  // request is k-anonymous and `false` means that the ID isn't k-anonymous. In
  // the event of an error, an empty vector will be passed to the callback.
  // There is no requirement that a user has joined an ID or has not joined and
  // ID to perform a query on that ID. The IDs are already hashed via SHA256.
  virtual void QuerySets(
      std::vector<std::string> ids,
      base::OnceCallback<void(std::vector<bool>)> callback) = 0;

  // The minimum period of time that a user of this interface should wait
  // between JoinSet calls with the same `id`.
  virtual base::TimeDelta GetJoinInterval() = 0;

  // The minimum period of time that a user of this interface should wait
  // between QuerySets calls including the same `id`.
  virtual base::TimeDelta GetQueryInterval() = 0;
};

}  // namespace content
#endif  // CONTENT_PUBLIC_BROWSER_K_ANONYMITY_SERVICE_DELEGATE_H_
