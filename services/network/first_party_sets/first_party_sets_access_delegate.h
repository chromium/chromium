// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_

#include <memory>
#include <optional>

#include "base/containers/circular_deque.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/types/optional_ref.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/first_party_sets/first_party_sets_cache_filter.h"
#include "services/network/first_party_sets/first_party_sets_manager.h"
#include "services/network/public/mojom/first_party_sets_access_delegate.mojom.h"

namespace net {
class FirstPartySetMetadata;
class FirstPartySetsContextConfig;
class SchemefulSite;
}  // namespace net

namespace network {

class FirstPartySetsAccessDelegate
    : public mojom::FirstPartySetsAccessDelegate {
 public:
  // Construct a FirstPartySetsAccessDelegate that provides customizations
  // and serves mojo requests for the underlying First-Party Sets info.
  // `*manager` outlives this object.
  FirstPartySetsAccessDelegate(
      mojo::PendingReceiver<mojom::FirstPartySetsAccessDelegate> receiver,
      mojom::FirstPartySetsAccessDelegateParamsPtr params,
      FirstPartySetsManager* const manager);

  FirstPartySetsAccessDelegate(const FirstPartySetsAccessDelegate&) = delete;
  FirstPartySetsAccessDelegate& operator=(const FirstPartySetsAccessDelegate&) =
      delete;

  ~FirstPartySetsAccessDelegate() override;

  // mojom::FirstPartySetsAccessDelegate
  void NotifyReady(mojom::FirstPartySetsReadyEventPtr ready_event) override;
  void SetEnabled(bool enabled) override;

  // Computes the First-Party Set metadata and cache filter match info related
  // to the given context. If `NotifyReady` has not yet been called or this
  // instance is disabled, returns default-constructed output.
  [[nodiscard]]
  std::pair<net::FirstPartySetMetadata,
            net::FirstPartySetsCacheFilter::MatchInfo>
  ComputeMetadata(const net::SchemefulSite& site,
                  base::optional_ref<const net::SchemefulSite> top_frame_site);

 private:
  net::FirstPartySetsContextConfig* context_config() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ready_event_.has_value() ? &(ready_event_.value()->config) : nullptr;
  }

  net::FirstPartySetsCacheFilter* cache_filter() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return ready_event_.has_value() ? &(ready_event_.value()->cache_filter)
                                    : nullptr;
  }

  // The underlying FirstPartySetsManager instance, which lives on the network
  // service.
  const raw_ptr<FirstPartySetsManager> manager_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Whether First-Party Sets is enabled for this context in particular. Note
  // that this is unrelated to `manager_.is_enabled`. This may be reassigned via
  // `SetEnabled`.
  bool enabled_ GUARDED_BY_CONTEXT(sequence_checker_);

  // The first ReadyEvent received. This is set at most once, and is immutable
  // thereafter.
  std::optional<mojom::FirstPartySetsReadyEventPtr> ready_event_
      GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Receiver<mojom::FirstPartySetsAccessDelegate> receiver_
      GUARDED_BY_CONTEXT(sequence_checker_){this};

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_FIRST_PARTY_SETS_ACCESS_DELEGATE_H_
