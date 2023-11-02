// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_NQE_EFFECTIVE_CONNECTION_TYPE_OBSERVER_H_
#define NET_NQE_EFFECTIVE_CONNECTION_TYPE_OBSERVER_H_

#include "base/compiler_specific.h"
#include "net/base/net_export.h"
#include "net/nqe/effective_connection_type.h"

namespace net {

// Observes changes in effective connection type.
class NET_EXPORT_PRIVATE EffectiveConnectionTypeObserver {
 public:
  EffectiveConnectionTypeObserver(const EffectiveConnectionTypeObserver&) =
      delete;
  EffectiveConnectionTypeObserver& operator=(
      const EffectiveConnectionTypeObserver&) = delete;

  // Notifies the observer of a change in the effective connection type.
  // NetworkQualityEstimator computes the effective connection type once in
  // every interval of duration
  // |effective_connection_type_recomputation_interval_|. Additionally, when
  // there is a change in the connection type of the device, then the
  // effective connection type is immediately recomputed.
  //
  // If the computed effective connection type is different from the
  // previously notified effective connection type, then all the registered
  // observers are notified of the new effective connection type.
  virtual void OnEffectiveConnectionTypeChanged(
      EffectiveConnectionType type) = 0;

 protected:
  EffectiveConnectionTypeObserver() = default;
  virtual ~EffectiveConnectionTypeObserver() = default;
};

}  // namespace net

#endif  // NET_NQE_EFFECTIVE_CONNECTION_TYPE_OBSERVER_H_
