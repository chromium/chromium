// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/url_request/url_request_error_job.h"

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"
#include "net/base/net_errors.h"

namespace net {

URLRequestErrorJob::URLRequestErrorJob(URLRequest* request, int error)
    : URLRequestJob(request), error_(error) {}

URLRequestErrorJob::~URLRequestErrorJob() = default;

void URLRequestErrorJob::Start() {
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&URLRequestErrorJob::StartAsync,
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
