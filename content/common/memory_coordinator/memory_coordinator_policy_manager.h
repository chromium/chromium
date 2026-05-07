// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
#define CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/functional/function_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory_coordinator/memory_consumer.h"
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

// Manages memory coordinator policies and aggregates their memory limit
// requests for consumer groups by multiplying them together.
class CONTENT_EXPORT MemoryCoordinatorPolicyManager
    : public MemoryConsumerGroupController {
 public:
  // An interface for observing the lifecycle of memory consumers.
  class CONTENT_EXPORT Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    // Called when a new consumer group is added/removed.
    virtual void OnConsumerGroupAdded(
        uint32_t consumer_id,
        std::string_view consumer_name,
        std::optional<base::MemoryConsumerTraits> traits,
        ProcessType process_type,
        ChildProcessId child_process_id) = 0;
    virtual void OnConsumerGroupRemoved(uint32_t consumer_id,
                                        ChildProcessId child_process_id) = 0;
  };

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  // An interface for observing diagnostic-only events. This is separate from
  // `Observer` because producing memory limit changed events has an associated
  // runtime cost. This way that cost is paid only when needed.
  class CONTENT_EXPORT DiagnosticObserver : public base::CheckedObserver {
   public:
    ~DiagnosticObserver() override = default;

    // Called when the aggregate memory limit for a consumer group changes.
    virtual void OnMemoryLimitChanged(uint32_t consumer_id,
                                      ChildProcessId child_process_id,
                                      int memory_limit) = 0;
  };
#endif

  MemoryCoordinatorPolicyManager();
  ~MemoryCoordinatorPolicyManager() override;

  MemoryCoordinatorPolicyManager(const MemoryCoordinatorPolicyManager&) =
      delete;
  MemoryCoordinatorPolicyManager& operator=(
      const MemoryCoordinatorPolicyManager&) = delete;

  // Adds/removes an observer.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

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
  void AddMemoryConsumerGroupHost(ChildProcessId child_process_id,
                                  MemoryConsumerGroupHost* host) override;
  void RemoveMemoryConsumerGroupHost(ChildProcessId child_process_id) override;
  void OnConsumerGroupAdded(uint32_t consumer_id,
                            std::string_view consumer_name,
                            std::optional<base::MemoryConsumerTraits> traits,
                            ProcessType process_type,
                            ChildProcessId child_process_id) override;
  void OnConsumerGroupRemoved(uint32_t consumer_id,
                              ChildProcessId child_process_id) override;
#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  void OnMemoryLimitChanged(uint32_t consumer_id,
                            ChildProcessId child_process_id,
                            int memory_limit) override;
#endif

  // Called by policies to request actions on multiple consumer groups across
  // potentially different child processes.
  void UpdateConsumers(MemoryCoordinatorPolicy* policy,
                       std::vector<GlobalMemoryConsumerUpdate> updates);

  // Called by policies to request actions on multiple consumer groups for the
  // current process.
  void UpdateConsumers(MemoryCoordinatorPolicy* policy,
                       std::vector<MemoryConsumerUpdate> updates);

  using ConsumerFilter =
      base::FunctionRef<bool(uint32_t consumer_id,
                             std::optional<base::MemoryConsumerTraits> traits,
                             ProcessType process_type,
                             ChildProcessId child_process_id)>;

  // Called by policies to request actions on all consumer groups that match the
  // filter.
  void UpdateConsumers(MemoryCoordinatorPolicy* policy,
                       ConsumerFilter filter,
                       std::optional<int> percentage,
                       bool release_memory);

  // Testing utilities ---------------------------------------------------------

  // Adds a memory limit override for the consumer named `consumer_name`.
  // This override takes precedence over any limits calculated by policies.
  // Fails a CHECK if an override already exists for this consumer.
  void AddMemoryLimitOverrideForTesting(std::string_view consumer_name,
                                        int percentage);

  // Updates an existing memory limit override for the consumer named
  // `consumer_name`. Fails a CHECK if an override does not exist for this
  // consumer.
  void UpdateMemoryLimitOverrideForTesting(std::string_view consumer_name,
                                           int percentage);

  // Clears the memory limit override for the consumer named `consumer_name`.
  void ClearMemoryLimitOverrideForTesting(std::string_view consumer_name);

  // Simulates a memory release request for the consumer named `consumer_name`.
  void NotifyReleaseMemoryForTesting(std::string_view consumer_name);

 private:
  class GroupState {
   public:
    GroupState(std::string_view consumer_name,
               std::optional<base::MemoryConsumerTraits> traits,
               ProcessType process_type);
    ~GroupState();

    // Updates the limit requested by `policy`. If `percentage` is 100, the
    // policy's limit is cleared. Returns the new aggregate limit if it changed,
    // or std::nullopt otherwise.
    std::optional<int> SetMemoryLimitForPolicy(MemoryCoordinatorPolicy* policy,
                                               int percentage);

    const std::string& consumer_name() const { return consumer_name_; }
    std::optional<base::MemoryConsumerTraits> traits() const { return traits_; }
    ProcessType process_type() const { return process_type_; }
    int current_limit() const { return current_limit_; }

    // Sets a memory limit override for testing. Returns the new effective
    // limit if it changed.
    std::optional<int> SetOverrideLimitForTesting(
        std::optional<int> percentage);

   private:
    int RecomputeMemoryLimit() const;

    const std::string consumer_name_;
    const std::optional<base::MemoryConsumerTraits> traits_;
    const ProcessType process_type_;

    // The limit requested by each policy.
    base::flat_map<MemoryCoordinatorPolicy*, int> requested_limits_;

    // The last memory limit that was applied to this group.
    int current_limit_ = base::MemoryConsumer::kDefaultMemoryLimit;

    // The memory limit override set for testing.
    std::optional<int> override_limit_;
  };

  struct HostState {
    explicit HostState(MemoryConsumerGroupHost* host);
    ~HostState();

    raw_ptr<MemoryConsumerGroupHost> host;
    absl::flat_hash_map<uint32_t, std::unique_ptr<GroupState>> groups;
  };

  HostState& GetHostState(ChildProcessId child_process_id);
  GroupState& GetGroupState(HostState& host_state, uint32_t consumer_id);

  void UpdateConsumersForProcess(MemoryCoordinatorPolicy* policy,
                                 ChildProcessId child_process_id,
                                 std::vector<MemoryConsumerUpdate> updates);

  // Applies the memory limit override to all registered consumers with the
  // given name.
  void ApplyMemoryLimitOverrideForTesting(std::string_view consumer_name,
                                          int percentage);

  base::ObserverList<Observer> observers_;

#if BUILDFLAG(ENABLE_MEMORY_COORDINATOR_INTERNALS)
  base::ObserverList<DiagnosticObserver> diagnostic_observers_;
#endif

  base::flat_set<MemoryCoordinatorPolicy*> policies_;

  absl::flat_hash_map<ChildProcessId, std::unique_ptr<HostState>> hosts_;

  // Overrides for specific consumers. These take precedence over limits
  // calculated by policies.
  base::flat_map<std::string /* consumer_name */, int, std::less<>>
      memory_limit_overrides_;
};

}  // namespace content

#endif  // CONTENT_COMMON_MEMORY_COORDINATOR_MEMORY_COORDINATOR_POLICY_MANAGER_H_
