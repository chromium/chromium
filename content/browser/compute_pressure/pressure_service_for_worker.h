// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_WORKER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_WORKER_H_

#include <type_traits>

#include "base/export_template.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/compute_pressure/pressure_service_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

class DedicatedWorkerHost;
class SharedWorkerHost;

template <typename WorkerHost>
class EXPORT_TEMPLATE_DECLARE(CONTENT_EXPORT) PressureServiceForWorker
    : public PressureServiceBase {
 public:
  explicit PressureServiceForWorker(WorkerHost* host) : worker_host_(host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  ~PressureServiceForWorker() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  PressureServiceForWorker(const PressureServiceForWorker&) = delete;
  PressureServiceForWorker& operator=(const PressureServiceForWorker&) = delete;

  // PressureServiceBase override.
  bool ShouldDeliverUpdate() const override;
  std::optional<base::UnguessableToken> GetTokenFor(
      device::mojom::PressureSource) const override;

 private:
  // DedicatedWorkerHost/SharedWorkerHost owns an instance of this class.
  raw_ptr<WorkerHost> GUARDED_BY_CONTEXT(sequence_checker_) worker_host_;
};

extern template class EXPORT_TEMPLATE_DECLARE(CONTENT_EXPORT)
    PressureServiceForWorker<DedicatedWorkerHost>;
extern template class EXPORT_TEMPLATE_DECLARE(CONTENT_EXPORT)
    PressureServiceForWorker<SharedWorkerHost>;

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_WORKER_H_
