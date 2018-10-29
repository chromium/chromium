// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_PAGE_RESOURCE_COORDINATOR_H_
#define SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_PAGE_RESOURCE_COORDINATOR_H_

#include "base/memory/weak_ptr.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "services/resource_coordinator/public/cpp/frame_resource_coordinator.h"
#include "services/resource_coordinator/public/cpp/resource_coordinator_interface.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace resource_coordinator {

class COMPONENT_EXPORT(SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP)
    PageResourceCoordinator
    : public ResourceCoordinatorInterface<mojom::PageCoordinationUnitPtr,
                                          mojom::PageCoordinationUnitRequest> {
 public:
  PageResourceCoordinator(service_manager::Connector* connector);
  ~PageResourceCoordinator() override;

  void SetIsLoading(bool is_loading);
  void SetVisibility(bool visible);
  void SetUKMSourceId(int64_t ukm_source_id);
  void OnFaviconUpdated();
  void OnTitleUpdated();
  void OnMainFrameNavigationCommitted(base::TimeTicks navigation_committed_time,
                                      uint64_t navigation_id,
                                      const std::string& url);

  void AddFrame(const FrameResourceCoordinator& frame);
  void RemoveFrame(const FrameResourceCoordinator& frame);

 private:
  void ConnectToService(mojom::CoordinationUnitProviderPtr& provider,
                        const CoordinationUnitID& cu_id) override;

  void AddFrameByID(const CoordinationUnitID& cu_id);
  void RemoveFrameByID(const CoordinationUnitID& cu_id);

  THREAD_CHECKER(thread_checker_);

  // The WeakPtrFactory should come last so the weak ptrs are invalidated
  // before the rest of the member variables.
  base::WeakPtrFactory<PageResourceCoordinator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PageResourceCoordinator);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_PUBLIC_CPP_PAGE_RESOURCE_COORDINATOR_H_
