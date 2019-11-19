// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_error_job.h"

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_status.h"

namespace net {

URLRequestErrorJob::URLRequestErrorJob(URLRequest* request,
                                       NetworkDelegate* network_delegate,
                                       int error)
    : URLRequestJob(request, network_delegate), error_(error) {}

URLRequestErrorJob::~URLRequestErrorJob() = default;

void URLRequestErrorJob::Start() {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestErrorJob::StartAsync,
                                weak_factory_.GetWeakPtr()));
}

void URLRequestErrorJob::Kill() {
  weak_factory_.InvalidateWeakPtrs();
  URLRequestJob::Kill();
}

void URLRequestErrorJob::StartAsync() {
  NotifyStartError(URLRequestStatus(URLRequestStatus::FAILED, error_));
}

}  // namespace net
