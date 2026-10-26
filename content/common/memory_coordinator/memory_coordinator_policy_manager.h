// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/auto_reset.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
#include "base/memory_coordinator/memory_limit.h"
#include "base/memory_coordinator/traits.h"
#include "base/observer_list.h"
#include "content/common/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/memory_coordinator/memory_consumer_group_controller.h"
#include "content/public/common/child_process_id.h"
#include "content/public/common/memory_consumer_update.h"
#include "content/public/common/process_type.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace content {

class MemoryCoordinatorPolicy;

// Represents a memory consumer update for a specific child process.
struct GlobalMemoryConsumerUpdate {
  ChildProcessId child_process_id;
  MemoryConsumerUpdate update;

  bool operator==(const GlobalMemoryConsumerUpdate&) const = default;
};

// Returns true if a consumer with `traits` operates in stateful mode, i.e. it
// maintains the memory limit it receives. Consumers operate in stateless mode
// regardless of their declared traits when base::kStatefulMemoryPressure is
// disabled.
CONTENT_EXPORT bool IsEffectivelyStateful(
    const base::MemoryConsumerTraits& traits);

// Manages memory coordinator policies and aggregates their memory limit
// requests for consumer groups by multiplying them together.
class CONTENT_EXPORT MemoryCoordinatorPolicyManager
    : public MemoryConsumerGroupController {
 public:
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // An interface for observing diagnostic-only events. This is separate from
  // policy notifications because producing memory limit changed events has an
  // associated runtime cost.
  class CONTENT_EXPORT DiagnosticObserver : public base::CheckedObserver {
   public:
    ~DiagnosticObserver() override = default;

    // Called when the aggregate memory limit for a consumer group changes.
    virtual void OnMemoryLimitChanged(uint32_t consumer_id,
                                      ChildProcessId child_process_id,
                                      base::MemoryLimit memory_limit) = 0;
  };
#endif

  MemoryCoordinatorPolicyManager();
  ~MemoryCoordinatorPolicyManager() override;

  MemoryCoordinatorPolicyManager(const MemoryCoordinatorPolicyManager&) =
      delete;
  MemoryCoordinatorPolicyManager& operator=(
      const MemoryCoordinatorPolicyManager&) = delete;

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // Adds/removes a diagnostic observer. Whenever there is one or more
  // diagnostic observer registered, additional diagnostic reporting is enabled.
  void AddDiagnosticObserver(DiagnosticObserver* observer);
  void RemoveDiagnosticObserver(DiagnosticObserver* observer);
#endif

  // Registers a policy with the manager. `policy` must remain valid until it is
  // removed with a call to RemovePolicy().
  void AddPolicy(MemoryCoordinatorPolicy* policy);

  // Unregisters a policy with the manager and clears all its associated data.
  void RemovePolicy(MemoryCoordinatorPolicy* policy);

  // MemoryConsumerGroupController:
  void AddMemoryConsumerGroupHost(ProcessType process_type,
                                  ChildProcessId child_process_id,
                                  MemoryConsumerGroupHost* host) override;
  void RemoveMemoryConsumerGroupHost(ChildProcessId child_process_id) override;
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            base::MemoryConsumerTraits traits,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override;
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void OnMemoryLimitChanged(uint32_t consumer_id,
                            ChildProcessId child_process_id,
                            base::MemoryLimit memory_limit) override;
#endif

  // Called by policies to request actions on multiple consumer groups across
  // potentially different child processes.
  void UpdateConsumers(MemoryCoordinatorPolicy* policy,
                       std::vector<GlobalMemoryConsumerUpdate> updates);

  // Called by policies to request actions on multiple consumer groups for the
  // current process.
  void UpdateConsumers(MemoryCoordinatorPolicy* policy,
                       std::vector<MemoryConsumerUpdate> updates);

  using ConsumerFilterRef =
      base::FunctionRef<bool(uint32_t consumer_id,
                             std::string_view consumer_name,
                             base::MemoryConsumerTraits traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id)>;

  // Called by policies to request actions on all consumer groups that match the
  // filter.
  void UpdateConsumers(MemoryCoordinatorPolicy* policy,
                       ConsumerFilterRef filter,
                       std::optional<base::MemoryLimit> memory_limit,
                       bool release_memory);

  // Release suppression -------------------------------------------------------

  using ConsumerFilter =
      base::RepeatingCallback<bool(uint32_t consumer_id,
                                   std::string_view consumer_name,
                                   base::MemoryConsumerTraits traits,
                                   ProcessType process_type,
                                   ChildProcessId child_process_id)>;

  // Called by `policy` to suppress memory release requests, from any policy,
  // for all consumer groups that match `filter`, including groups added later.
  // Memory limits are still applied, which allows a policy to stop the growth
  // of consumers without asking them to reduce their footprint. Replaces the
  // filter previously set by `policy`, if any. A group stays suppressed as long
  // as at least one policy suppresses it.
  //
  // Release requests that are suppressed are dropped. When a group is no
  // longer suppressed by any policy, and a release request was dropped, it is
  // asked to release memory so that it converges to its aggregate memory
  // limit. For stateless groups (see IsEffectivelyStateful()), this is only
  // done if their limit is below the default. For stateful groups, this is only
  // done if their limit is lower than the highest limit applied since they last
  // released memory, as their consumers can't be above that.
  //
  // `policy` must be registered. Must not be called while policies are being
  // notified (e.g. from MemoryCoordinatorPolicy::OnConsumerGroupAdded()).
  void SetMemoryReleaseSuppression(MemoryCoordinatorPolicy* policy,
                                   ConsumerFilter filter);

  // Called by `policy` to stop the suppression it set with
  // SetMemoryReleaseSuppression(). No-op if `policy` doesn't suppress memory
  // releases. Also done automatically by RemovePolicy(). Must not be called
  // while policies are being notified.
  void ClearMemoryReleaseSuppression(MemoryCoordinatorPolicy* policy);

  // Override utilities --------------------------------------------------------

  // Sets or updates a memory limit override for the consumer with the given ID.
  // This override takes precedence over any limits calculated by policies.
  void SetMemoryLimitOverride(uint32_t consumer_id,
                              base::MemoryLimit memory_limit);

  // Clears the memory limit override for the consumer with the given ID.
  // Fails a CHECK if an override does not exist for this consumer.
  void ClearMemoryLimitOverride(uint32_t consumer_id);

  // Simulates a memory release request for the consumer with the given ID.
  // Intentionally ignores memory release suppression.
  void NotifyReleaseMemoryForTesting(uint32_t consumer_id);

 private:
  class GroupState {
   public:
    GroupState(std::string_view consumer_name,
               base::MemoryConsumerTraits traits);
    ~GroupState();

    // Updates the limit requested by `policy`. If `memory_limit` is the
    // default, the policy's limit is cleared. Returns the new aggregate limit
    // if it changed, or std::nullopt otherwise.
    std::optional<base::MemoryLimit> SetMemoryLimitForPolicy(
        MemoryCoordinatorPolicy* policy,
        base::MemoryLimit memory_limit);

    const std::string& consumer_name() const { return consumer_name_; }
    base::MemoryConsumerTraits traits() const { return traits_; }
    base::MemoryLimit current_limit() const { return current_limit_; }

    // Sets a memory limit override. Returns the new effective limit if it
    // changed.
    std::optional<base::MemoryLimit> SetOverrideLimit(
        std::optional<base::MemoryLimit> memory_limit);

    // Makes `policy` suppress memory release requests for this group. Repeated
    // calls are allowed.
    void SetMemoryReleaseSuppression(MemoryCoordinatorPolicy* policy);

    // Stops `policy` from suppressing memory release requests for this group.
    // No-op if it doesn't. Returns true if this removed the last suppression
    // and a release request that was dropped while suppressed should now be
    // sent. The caller must then send it.
    [[nodiscard]] bool ClearMemoryReleaseSuppression(
        MemoryCoordinatorPolicy* policy);

    // Returns true if memory release requests are suppressed for this group.
    bool IsMemoryReleaseSuppressed() const {
      return !memory_release_suppressors_.empty();
    }

    // Records that a memory release request was dropped because releases are
    // suppressed for this group.
    void SetHasPendingRelease() { has_pending_release_ = true; }

    // Must be called when a memory release request is sent to the host for
    // this group.
    void OnReleaseSent();

   private:
    base::MemoryLimit RecomputeMemoryLimit() const;

    // Updates `current_limit_` and `peak_limit_since_release_`.
    void SetCurrentLimit(base::MemoryLimit memory_limit);

    const std::string consumer_name_;
    const base::MemoryConsumerTraits traits_;

    // The limit requested by each policy.
    base::flat_map<MemoryCoordinatorPolicy*, base::MemoryLimit>
        requested_limits_;

    // The last memory limit that was applied to this group.
    base::MemoryLimit current_limit_ = base::MemoryLimit::Default();

    // The memory limit override.
    std::optional<base::MemoryLimit> override_limit_;

    // The policies currently suppressing memory release requests for this
    // group.
    base::flat_set<MemoryCoordinatorPolicy*> memory_release_suppressors_;

    // True if a memory release request was dropped while suppressed.
    bool has_pending_release_ = false;

    // The highest limit applied to this group since the last memory release
    // request was sent. A stateful consumer respects its limit and can't use
    // more memory than this, so a dropped release request only needs to be
    // replayed if the current limit is lower. Not used for stateless groups,
    // which ignore limit updates.
    base::MemoryLimit peak_limit_since_release_ = base::MemoryLimit::Default();
  };

  struct HostState {
    HostState(MemoryConsumerGroupHost* host, ProcessType process_type);
    ~HostState();

    raw_ptr<MemoryConsumerGroupHost> host;
    const ProcessType process_type;
    absl::flat_hash_map<uint32_t, std::unique_ptr<GroupState>> groups;
  };

  HostState& GetHostState(ChildProcessId child_process_id);
  GroupState& GetGroupState(HostState& host_state, uint32_t consumer_id);

  void UpdateConsumersForProcess(MemoryCoordinatorPolicy* policy,
                                 ChildProcessId child_process_id,
                                 std::vector<MemoryConsumerUpdate> updates);

  // Applies the memory limit override to all registered consumers with the
  // given ID.
  void ApplyMemoryLimitOverride(uint32_t consumer_id,
                                base::MemoryLimit memory_limit);

  // Makes `policy` suppress memory release requests for all groups that match
  // `filter`, and stop suppressing them for the other groups. A null `filter`
  // matches no group. Groups that are no longer suppressed by any policy are
  // asked to release memory if a release request was dropped and is still
  // needed.
  void ApplyMemoryReleaseSuppression(MemoryCoordinatorPolicy* policy,
                                     const ConsumerFilter* filter);

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  base::ObserverList<DiagnosticObserver> diagnostic_observers_;
#endif

  bool is_notifying_ = false;
  base::flat_set<raw_ptr<MemoryCoordinatorPolicy>> policies_;

  absl::flat_hash_map<ChildProcessId, std::unique_ptr<HostState>> hosts_;

  // Overrides for specific consumers. These take precedence over limits
  // calculated by policies.
  base::flat_map<uint32_t /* consumer_id */, base::MemoryLimit>
      memory_limit_overrides_;

  // The memory release suppression filter set by each policy. Applied to
  // consumer groups as they are added.
  base::flat_map<MemoryCoordinatorPolicy*, ConsumerFilter>
      memory_release_suppression_filters_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
