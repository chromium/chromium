// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/optional_ref.h"
#include "net/base/schemeful_site.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/first_party_set_metadata.h"
#include "net/first_party_sets/first_party_sets_context_config.h"
#include "net/first_party_sets/global_first_party_sets.h"

namespace network {

// Class FirstPartySetsManager is a pseudo-singleton owned by NetworkService; it
// answers queries about First-Party Sets after they've been loaded.
class FirstPartySetsManager {
 public:
  explicit FirstPartySetsManager(bool enabled);
  ~FirstPartySetsManager();

  FirstPartySetsManager(const FirstPartySetsManager&) = delete;
  FirstPartySetsManager& operator=(const FirstPartySetsManager&) = delete;

  bool is_enabled() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return enabled_;
  }

  // Computes the First-Party Set metadata related to the given request context.
  // If `SetCompleteSets` has not yet been called, returns a default-constructed
  // `FirstPartySetMetadata` instance.
  [[nodiscard]] net::FirstPartySetMetadata ComputeMetadata(
      const net::SchemefulSite& site,
      base::optional_ref<const net::SchemefulSite> top_frame_site,
      const net::FirstPartySetsContextConfig& fps_context_config);

  // Stores the First-Party Sets data.
  //
  // Only the first call to SetCompleteSets can have any effect; subsequent
  // invocations are ignored.
  void SetCompleteSets(net::GlobalFirstPartySets sets);

 private:
  // The global First-Party Sets data.
  //
  // Optional because it is unset until the data has been received from the
  // browser process.
  std::optional<net::GlobalFirstPartySets> sets_
      GUARDED_BY_CONTEXT(sequence_checker_);

  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<FirstPartySetsManager> weak_factory_{this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_MANAGER_H_
