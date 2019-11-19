// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_ASYNC_API_FUNCTION_H_
#define EXTENSIONS_BROWSER_API_ASYNC_API_FUNCTION_H_

#include "base/memory/ref_counted.h"
#include "base/sequenced_task_runner.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

// AsyncApiFunction provides convenient thread management for APIs that need to
// do essentially all their work on a thread other than the UI thread.
class AsyncApiFunction : public ExtensionFunction {
 protected:
  AsyncApiFunction();
  ~AsyncApiFunction() override;

  // Like Prepare(). A useful place to put common work in an ApiFunction
  // superclass that multiple API functions want to share.
  virtual bool PrePrepare();

  // Set up for work (e.g., validate arguments). Guaranteed to happen on UI
  // thread.
  virtual bool Prepare() = 0;

  // Do actual work. Guaranteed to happen on the task runner specified in
  // |work_task_runner_| if non-null; or on the IO thread otherwise.
  virtual void Work();

  // Start the asynchronous work. Guraranteed to happen on work thread.
  virtual void AsyncWorkStart();

  // Respond. Guaranteed to happen on UI thread.
  virtual bool Respond() = 0;

  // ExtensionFunction:
  ResponseAction Run() final;

 protected:
  // ValidationFailure override to match RunAsync().
  static bool ValidationFailure(AsyncApiFunction* function);

  scoped_refptr<base::SequencedTaskRunner> work_task_runner() const {
    return work_task_runner_;
  }
  void set_work_task_runner(
      scoped_refptr<base::SequencedTaskRunner> work_task_runner) {
    work_task_runner_ = work_task_runner;
  }

  // Notify AsyncIOApiFunction that the work is completed
  void AsyncWorkCompleted();

  // Sets a single Value as the results of the function.
  void SetResult(std::unique_ptr<base::Value> result);

  // Sets multiple Values as the results of the function.
  void SetResultList(std::unique_ptr<base::ListValue> results);

  void SetError(const std::string& error);
  const std::string& GetError() const override;

  // Responds with success/failure. |results_| or |error_| should be set
  // accordingly.
  void SendResponse(bool success);

  // Exposed versions of |results_| and |error_| which are curried into the
  // ExtensionFunction response.
  // These need to keep the same name to avoid breaking existing
  // implementations, but this should be temporary with https://crbug.com/648275
  // and https://crbug.com/634140.
  std::unique_ptr<base::ListValue> results_;
  std::string error_;

 private:
  void WorkOnWorkThread();
  void RespondOnUIThread();

  // Return true to indicate that nothing has gone wrong yet; SendResponse must
  // be called later. Return false to respond immediately with an error.
  bool RunAsync();

  // If you don't want your Work() method to happen on the IO thread, then set
  // this to the SequenceTaskRunner you do want to use, preferably in Prepare().
  scoped_refptr<base::SequencedTaskRunner> work_task_runner_;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_ASYNC_API_FUNCTION_H_
