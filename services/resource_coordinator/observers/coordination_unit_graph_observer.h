// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_OBSERVER_H_
#define SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_OBSERVER_H_

#include "base/macros.h"
#include "services/resource_coordinator/public/mojom/coordination_unit.mojom.h"

namespace resource_coordinator {

class CoordinationUnitBase;
class CoordinationUnitGraph;
class FrameCoordinationUnitImpl;
class PageCoordinationUnitImpl;
class ProcessCoordinationUnitImpl;
class SystemCoordinationUnitImpl;

// An observer API for the coordination unit graph maintained by GRC.
//
// Observers are instantiated when the resource_coordinator service
// is created and are destroyed when the resource_coordinator service
// is destroyed. Therefore observers are guaranteed to be alive before
// any coordination unit is created and will be alive after any
// coordination unit is destroyed. Additionally, any
// Coordination Unit reachable within a callback will always be
// initialized and valid.
//
// To create and install a new observer:
//   (1) Derive from this class.
//   (2) Register by calling on |coordination_unit_graph().RegisterObserver|
//       inside of the ResourceCoordinatorService::Create.
class CoordinationUnitGraphObserver {
 public:
  CoordinationUnitGraphObserver();
  virtual ~CoordinationUnitGraphObserver();

  // Determines whether or not the observer should be registered with, and
  // invoked for, the |coordination_unit|.
  virtual bool ShouldObserve(const CoordinationUnitBase* coordination_unit) = 0;

  // Called whenever a CoordinationUnit is created.
  virtual void OnCoordinationUnitCreated(
      const CoordinationUnitBase* coordination_unit) {}

  // Called when the |coordination_unit| is about to be destroyed.
  virtual void OnBeforeCoordinationUnitDestroyed(
      const CoordinationUnitBase* coordination_unit) {}

  // Called whenever a property of the |coordination_unit| is changed if the
  // |coordination_unit| doesn't implement its own PropertyChanged handler.
  virtual void OnPropertyChanged(const CoordinationUnitBase* coordination_unit,
                                 const mojom::PropertyType property_type,
                                 int64_t value) {}

  // Called whenever a property of the FrameCoordinationUnit is changed.
  virtual void OnFramePropertyChanged(const FrameCoordinationUnitImpl* frame_cu,
                                      const mojom::PropertyType property_type,
                                      int64_t value) {}

  // Called whenever a property of the PageCoordinationUnit is changed.
  virtual void OnPagePropertyChanged(const PageCoordinationUnitImpl* page_cu,
                                     const mojom::PropertyType property_type,
                                     int64_t value) {}

  // Called whenever a property of the ProcessCoordinationUnit is changed.
  virtual void OnProcessPropertyChanged(
      const ProcessCoordinationUnitImpl* process_cu,
      const mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever a property of the SystemCoordinationUnit is changed.
  virtual void OnSystemPropertyChanged(
      const SystemCoordinationUnitImpl* system_cu,
      const mojom::PropertyType property_type,
      int64_t value) {}

  // Called whenever an event is received in |coordination_unit| if the
  // |coordination_unit| doesn't implement its own EventReceived handler.
  virtual void OnEventReceived(const CoordinationUnitBase* coordination_unit,
                               const mojom::Event event) {}
  virtual void OnFrameEventReceived(const FrameCoordinationUnitImpl* frame_cu,
                                    const mojom::Event event) {}
  virtual void OnPageEventReceived(const PageCoordinationUnitImpl* page_cu,
                                   const mojom::Event event) {}
  virtual void OnProcessEventReceived(
      const ProcessCoordinationUnitImpl* process_cu,
      const mojom::Event event) {}
  virtual void OnSystemEventReceived(
      const SystemCoordinationUnitImpl* system_cu,
      const mojom::Event event) {}

  // Called when all the frames in a process become frozen.
  virtual void OnAllFramesInProcessFrozen(
      const ProcessCoordinationUnitImpl* process_cu) {}

  void set_coordination_unit_graph(
      CoordinationUnitGraph* coordination_unit_graph) {
    coordination_unit_graph_ = coordination_unit_graph;
  }

  const CoordinationUnitGraph& coordination_unit_graph() const {
    return *coordination_unit_graph_;
  }

 private:
  CoordinationUnitGraph* coordination_unit_graph_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(CoordinationUnitGraphObserver);
};

}  // namespace resource_coordinator

#endif  // SERVICES_RESOURCE_COORDINATOR_COORDINATION_UNIT_COORDINATION_UNIT_GRAPH_OBSERVER_H_
