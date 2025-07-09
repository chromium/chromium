// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_error_job.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/features.h"
#include "net/base/net_errors.h"
#include "net/base/task/task_runner.h"

namespace net {

namespace {
const scoped_refptr<base::SingleThreadTaskRunner>& TaskRunner(
    net::RequestPriority priority) {
  if (features::kNetTaskSchedulerURLRequestErrorJob.Get()) {
    return net::GetTaskRunner(priority);
  }
  return base::SingleThreadTaskRunner::GetCurrentDefault();
}

}  // namespace

URLRequestErrorJob::URLRequestErrorJob(URLRequest* request, int error)
    : URLRequestJob(request), error_(error) {}

URLRequestErrorJob::~URLRequestErrorJob() = default;

void URLRequestErrorJob::Start() {
  TaskRunner(request_->priority())
      ->PostTask(FROM_HERE, base::BindOnce(&URLRequestErrorJob::StartAsync,
                                           weak_factory_.GetWeakPtr()));
}

void URLRequestErrorJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  URLRequestJob::Kill();
}

void URLRequestErrorJob::StartAsync() {
  NotifyStartError(error_);
}

}  // namespace net
