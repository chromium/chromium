// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/async_api_function.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"

using content::BrowserThread;

namespace extensions {

// AsyncApiFunction
AsyncApiFunction::AsyncApiFunction()
    : work_task_runner_(
          base::CreateSingleThreadTaskRunner({BrowserThread::IO})) {}

AsyncApiFunction::~AsyncApiFunction() {}

bool AsyncApiFunction::RunAsync() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!PrePrepare() || !Prepare()) {
    return false;
  }
  bool rv = work_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&AsyncApiFunction::WorkOnWorkThread, this));
  DCHECK(rv);
  return true;
}

bool AsyncApiFunction::PrePrepare() {
  return true;
}

void AsyncApiFunction::Work() {}

void AsyncApiFunction::AsyncWorkStart() {
  Work();
  AsyncWorkCompleted();
}

// static
bool AsyncApiFunction::ValidationFailure(AsyncApiFunction* function) {
  return false;
}

ExtensionFunction::ResponseAction AsyncApiFunction::Run() {
  if (RunAsync())
    return RespondLater();
  DCHECK(!results_);
  return RespondNow(Error(error_));
}

void AsyncApiFunction::AsyncWorkCompleted() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    bool rv = base::PostTask(
        FROM_HERE, {BrowserThread::UI},
        base::BindOnce(&AsyncApiFunction::RespondOnUIThread, this));
    DCHECK(rv);
  } else {
    SendResponse(Respond());
  }
}

void AsyncApiFunction::SetResult(std::unique_ptr<base::Value> result) {
  results_.reset(new base::ListValue());
  results_->Append(std::move(result));
}

void AsyncApiFunction::SetResultList(std::unique_ptr<base::ListValue> results) {
  results_ = std::move(results);
}

void AsyncApiFunction::SetError(const std::string& error) {
  error_ = error;
}

const std::string& AsyncApiFunction::GetError() const {
  return error_.empty() ? ExtensionFunction::GetError() : error_;
}

void AsyncApiFunction::WorkOnWorkThread() {
  DCHECK(work_task_runner_->RunsTasksInCurrentSequence());
  AsyncWorkStart();
}

void AsyncApiFunction::RespondOnUIThread() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  SendResponse(Respond());
}

void AsyncApiFunction::SendResponse(bool success) {
  ResponseValue response;
  if (success) {
    response = ArgumentList(std::move(results_));
  } else {
    response = results_ ? ErrorWithArguments(std::move(results_), error_)
                        : Error(error_);
  }
  ExtensionFunction::Respond(std::move(response));
}

}  // namespace extensions
