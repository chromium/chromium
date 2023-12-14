// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_WORKER_H_
#define CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_WORKER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/compute_pressure/pressure_service_base.h"
#include "content/common/content_export.h"
#include "content/public/browser/browser_thread.h"

namespace content {

template <typename WorkerHost>
class CONTENT_EXPORT PressureServiceForWorker : public PressureServiceBase {
 public:
  explicit PressureServiceForWorker(WorkerHost* host) : worker_host_(host) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
  }

  ~PressureServiceForWorker() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  }

  PressureServiceForWorker(const PressureServiceForWorker&) = delete;
  PressureServiceForWorker& operator=(const PressureServiceForWorker&) = delete;

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  // DedicatedWorkerHost/SharedWorkerHost owns an instance of this class.
  raw_ptr<WorkerHost> GUARDED_BY_CONTEXT(sequence_checker_) worker_host_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPUTE_PRESSURE_PRESSURE_SERVICE_FOR_WORKER_H_
