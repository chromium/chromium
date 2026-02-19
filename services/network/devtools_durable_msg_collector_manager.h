// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_MANAGER_H_
#define SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_MANAGER_H_

#include <memory>
#include <set>

#include "base/memory/memory_pressure_listener.h"
#include "base/time/time.h"
#include "base/trace_event/memory_dump_provider.h"
#include "services/network/devtools_durable_msg_collector.h"
#include "services/network/public/mojom/network_service.mojom.h"

namespace network {

// DevtoolsDurableMessageCollectorManager class is responsible for managing
// lifetime of self owned collector instances and associating each to
// different DevTools Profiles.
class COMPONENT_EXPORT(NETWORK_SERVICE) DevtoolsDurableMessageCollectorManager
    : public base::trace_event::MemoryDumpProvider {
 public:
  // Allow certain friendship to call back on OnCollector* protected methods.
  friend class DevtoolsDurableMessageCollector;
  FRIEND_TEST_ALL_PREFIXES(DevtoolsDurableMessageCollectorManagerTest,
                           ReportAggregateMemoryUsage);

  DevtoolsDurableMessageCollectorManager();
  ~DevtoolsDurableMessageCollectorManager() override;

  // base::trace_event::MemoryDumpProvider implementation:
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  // Create a self-owned collector instance and bind it to the mojo pipe
  // provided. The collector and all data associated with it will be destroyed
  // on the mojo being tore down.
  void AddCollector(
      mojo::PendingReceiver<network::mojom::DurableMessageCollector> receiver);

  // Enable or disable response body collection on the provided DevTools
  // profile.
  void EnableForProfile(const base::UnguessableToken& profile_id,
                        DevtoolsDurableMessageCollector& collector);
  void DisableForProfile(const base::UnguessableToken& profile_id,
                         DevtoolsDurableMessageCollector& collector);

  // Get a list of collectors enabled for the given DevTools profile.
  std::vector<DevtoolsDurableMessageCollector*> GetCollectorsEnabledForProfile(
      const base::UnguessableToken& profile_id);

  std::vector<DevtoolsDurableMessageCollector*> GetCollectorsForTesting() {
    return std::vector<DevtoolsDurableMessageCollector*>(collectors_.begin(),
                                                         collectors_.end());
  }

  size_t total_memory_usage_for_testing() const { return total_memory_usage_; }

  // Returns true if the given size can be accommodated by the global limit.
  bool CanAccommodate(size_t size) const;

 private:
  // Callback by collector instances to inform of creation/destruction.
  void OnCollectorCreated(DevtoolsDurableMessageCollector* collector);
  void OnCollectorDestroyed(DevtoolsDurableMessageCollector* collector);
  void OnCollectorAddedBytes(size_t delta);
  void OnCollectorRemovedBytes(size_t delta);
  void OnCollectorAddedMessage(size_t count = 1);
  void OnCollectorRemovedMessage(size_t count = 1);

  // Keeps track of aggregate memory usage of all managed collectors, for
  // reporting purposes.
  size_t total_memory_usage_ = 0;
  size_t total_message_count_ = 0;

  // A set of collectors managed by this class.
  std::set<raw_ptr<DevtoolsDurableMessageCollector>> collectors_;

  // Keeps track of collectors being attached to a DevTools profile.
  std::multimap<const base::UnguessableToken,
                raw_ptr<DevtoolsDurableMessageCollector>>
      profile_collectors_;

  // Cached global limit buffer size.
  const uint64_t max_global_buffer_size_;

  base::WeakPtrFactory<DevtoolsDurableMessageCollectorManager> weak_factory_{
      this};
};

}  // namespace network

#endif  // SERVICES_NETWORK_DEVTOOLS_DURABLE_MSG_COLLECTOR_MANAGER_H_
