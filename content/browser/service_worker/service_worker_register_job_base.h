// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_BASE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_BASE_H_

namespace content {

class ServiceWorkerRegisterJobBase {
 public:
  enum RegistrationJobType {
    REGISTRATION_JOB,
    UNREGISTRATION_JOB,
    UPDATE_JOB
  };

  virtual ~ServiceWorkerRegisterJobBase() {}

  // Starts the job. This method should be called once and only once per job.
  virtual void Start() = 0;

  // Aborts the job. This method should be called once and only once per job.
  // It can be called regardless of whether Start() was called.
  virtual void Abort() = 0;

  // Returns true if this job is identical to |job| for the purpose of
  // collapsing them together in a ServiceWorkerJobCoordinator queue.
  // Registration jobs are equal if they are for the same scope and script
  // URL; unregistration jobs are equal if they are for the same scope.
  virtual bool Equals(ServiceWorkerRegisterJobBase* job) const = 0;

  // Returns the type of this job.
  virtual RegistrationJobType GetType() const = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_REGISTER_JOB_BASE_H_
