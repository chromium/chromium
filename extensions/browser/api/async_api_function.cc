// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/async_api_function.h"

#include <memory>

#include "base/functional/bind.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_system.h"

using content::BrowserThread;

namespace extensions {

// AsyncApiFunction
AsyncApiFunction::AsyncApiFunction()
    : work_task_runner_(content::GetIOThreadTaskRunner({})) {}

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
    bool rv = content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&AsyncApiFunction::RespondOnUIThread, this));
    DCHECK(rv);
  } else {
    SendResponse(Respond());
  }
}

void AsyncApiFunction::SetResult(base::Value result) {
  results_.emplace();
  results_->Append(std::move(result));
}

void AsyncApiFunction::SetResultList(base::Value::List results) {
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
  base::Value::List arguments;
  if (results_) {
    arguments = std::move(*results_);
    results_.reset();
  }
  if (success) {
    response = ArgumentList(std::move(arguments));
  } else if (results_) {
    response = ErrorWithArguments(std::move(arguments), error_);
  } else {
    response = Error(error_);
  }
  ExtensionFunction::Respond(std::move(response));
}

}  // namespace extensions
