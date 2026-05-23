// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_SHARED_WORKER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_SHARED_WORKER_H_

#include <type_traits>

#include "base/memory/raw_ptr.h"
#include "base/thread_annotations.h"
#include "content/browser/compute_pressure/pressure_service_base.h"
#include "content/common/content_export.h"

namespace content {

class SharedWorkerHost;

class CONTENT_EXPORT PressureServiceForSharedWorker
    : public PressureServiceBase {
 public:
  explicit PressureServiceForSharedWorker(SharedWorkerHost* host);

  ~PressureServiceForSharedWorker() override;

  PressureServiceForSharedWorker(const PressureServiceForSharedWorker&) =
      delete;
  PressureServiceForSharedWorker& operator=(
      const PressureServiceForSharedWorker&) = delete;

  // PressureServiceBase overrides.
  bool ShouldDeliverUpdate() const override;
  std::optional<base::UnguessableToken> GetTokenFor(
      device::mojom::PressureSource) const override;

 private:
  // SharedWorkerHost owns an instance of this class.
  raw_ptr<SharedWorkerHost> GUARDED_BY_CONTEXT(sequence_checker_) worker_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_SHARED_WORKER_H_
