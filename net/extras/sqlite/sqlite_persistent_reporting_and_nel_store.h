// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_
#define NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_

#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/task/task_traits.h"
#include "net/network_error_logging/network_error_logging_service.h"
#include "net/network_error_logging/persistent_reporting_and_nel_store.h"
#include "net/reporting/reporting_cache.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {

// Returns recommended task priority for |background_task_runner|.
base::TaskPriority COMPONENT_EXPORT(NET_EXTRAS)
    GetReportingAndNelStoreBackgroundSequencePriority();

class COMPONENT_EXPORT(NET_EXTRAS) SQLitePersistentReportingAndNelStore
    : public PersistentReportingAndNelStore {
 public:
  SQLitePersistentReportingAndNelStore(
      const base::FilePath& path,
      const scoped_refptr<base::SequencedTaskRunner>& client_task_runner,
      const scoped_refptr<base::SequencedTaskRunner>& background_task_runner);

  ~SQLitePersistentReportingAndNelStore() override;

  // NetworkErrorLoggingService::PersistentNelStore implementation
  void LoadNelPolicies(NelPoliciesLoadedCallback loaded_callback) override;
  void AddNelPolicy(
      const NetworkErrorLoggingService::NelPolicy& policy) override;
  void UpdateNelPolicyAccessTime(
      const NetworkErrorLoggingService::NelPolicy& policy) override;
  void DeleteNelPolicy(
      const NetworkErrorLoggingService::NelPolicy& policy) override;

  // ReportingCache::PersistentReportingStore implementation
  void LoadReportingClients(
      ReportingClientsLoadedCallback loaded_callback) override;
  void AddReportingEndpoint(const ReportingEndpoint& endpoint) override;
  void AddReportingEndpointGroup(
      const CachedReportingEndpointGroup& group) override;
  void UpdateReportingEndpointGroupAccessTime(
      const CachedReportingEndpointGroup& group) override;
  void UpdateReportingEndpointDetails(
      const ReportingEndpoint& endpoint) override;
  void UpdateReportingEndpointGroupDetails(
      const CachedReportingEndpointGroup& group) override;
  void DeleteReportingEndpoint(const ReportingEndpoint& endpoint) override;
  void DeleteReportingEndpointGroup(
      const CachedReportingEndpointGroup& group) override;

  void Flush() override;

  size_t GetQueueLengthForTesting() const;

 private:
  class Backend;

  // Calls |callback| with the loaded |policies|.
  void CompleteLoadNelPolicies(
      NelPoliciesLoadedCallback callback,
      std::vector<NetworkErrorLoggingService::NelPolicy> policies);

  // Calls |callback| with the loaded |endpoints| and |endpoint_groups|.
  void CompleteLoadReportingClients(
      ReportingClientsLoadedCallback callback,
      std::vector<ReportingEndpoint> endpoints,
      std::vector<CachedReportingEndpointGroup> endpoint_groups);

  const scoped_refptr<Backend> backend_;

  base::WeakPtrFactory<SQLitePersistentReportingAndNelStore> weak_factory_{
      this};

  DISALLOW_COPY_AND_ASSIGN(SQLitePersistentReportingAndNelStore);
};

}  // namespace net

#endif  // NET_EXTRAS_SQLITE_SQLITE_PERSISTENT_REPORTING_AND_NEL_STORE_H_
