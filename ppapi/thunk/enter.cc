// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/thunk/enter.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/task/single_thread_task_runner.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/tracked_callback.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/resource_creation_api.h"

namespace ppapi {
namespace {

bool IsMainThread() {
  return
      PpapiGlobals::Get()->GetMainThreadMessageLoop()->BelongsToCurrentThread();
}

bool CurrentThreadHandlingBlockingMessage() {
  ppapi::MessageLoopShared* current =
      PpapiGlobals::Get()->GetCurrentMessageLoop();
  return current && current->CurrentlyHandlingBlockingMessage();
}

}  // namespace

namespace thunk {

namespace subtle {

EnterBase::EnterBase() {}

EnterBase::EnterBase(PP_Resource resource) : resource_(GetResource(resource)) {}

EnterBase::EnterBase(PP_Instance instance, SingletonResourceID resource_id)
    : resource_(GetSingletonResource(instance, resource_id)) {
  if (!resource_)
    retval_ = PP_ERROR_BADARGUMENT;
}

EnterBase::EnterBase(PP_Resource resource,
                     const PP_CompletionCallback& callback)
    : EnterBase(resource) {
  callback_ = new TrackedCallback(resource_, callback);
}

EnterBase::EnterBase(PP_Instance instance,
                     SingletonResourceID resource_id,
                     const PP_CompletionCallback& callback)
    : EnterBase(instance, resource_id) {
  callback_ = new TrackedCallback(resource_, callback);
}

EnterBase::~EnterBase() {
  // callback_ is cleared any time it is run, scheduled to be run, or once we
  // know it will be completed asynchronously. So by this point it should be
  // null.
  DCHECK(!callback_) << "|callback_| is not null. Did you forget to call "
                        "|EnterBase::SetResult| in the interface's thunk?";
}

int32_t EnterBase::SetResult(int32_t result) {
  if (!callback_) {
    // It doesn't make sense to call SetResult if there is no callback.
    NOTREACHED();
  }
  if (result == PP_OK_COMPLETIONPENDING) {
    retval_ = result;
    if (callback_->is_blocking()) {
      DCHECK(!IsMainThread());  // We should have returned an error before this.
      retval_ = callback_->BlockUntilComplete();
    } else {
      // The callback is not blocking and the operation will complete
      // asynchronously, so there's nothing to do.
      retval_ = result;
    }
  } else {
    // The function completed synchronously.
    if (callback_->is_required()) {
      // This is a required callback, so we must issue it asynchronously.
      callback_->PostRun(result);
      retval_ = PP_OK_COMPLETIONPENDING;
    } else {
      // The callback is blocking or optional, so all we need to do is mark
      // the callback as completed so that it won't be issued later.
      callback_->MarkAsCompleted();
      retval_ = result;
    }
  }
  callback_ = nullptr;
  return retval_;
}

// static
Resource* EnterBase::GetResource(PP_Resource resource) {
  return PpapiGlobals::Get()->GetResourceTracker()->GetResource(resource);
}

// static
Resource* EnterBase::GetSingletonResource(PP_Instance instance,
                                          SingletonResourceID resource_id) {
  PPB_Instance_API* ppb_instance =
      PpapiGlobals::Get()->GetInstanceAPI(instance);
  if (!ppb_instance)
    return nullptr;

  return ppb_instance->GetSingletonResource(instance, resource_id);
}

void EnterBase::SetStateForCallbackError(bool report_error) {
  if (PpapiGlobals::Get()->IsHostGlobals()) {
    // In-process plugins can't make PPAPI calls off the main thread.
    CHECK(IsMainThread());
  }
  if (callback_) {
    if (callback_->is_blocking() && IsMainThread()) {
      // Blocking callbacks are never allowed on the main thread.
      callback_->MarkAsCompleted();
      callback_ = nullptr;
      retval_ = PP_ERROR_BLOCKS_MAIN_THREAD;
      if (report_error) {
        std::string message(
            "Blocking callbacks are not allowed on the main thread.");
        PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                    std::string(), message);
      }
    } else if (callback_->is_blocking() &&
               CurrentThreadHandlingBlockingMessage()) {
      // Blocking callbacks are not allowed while handling a blocking message.
      callback_->MarkAsCompleted();
      callback_ = nullptr;
      retval_ = PP_ERROR_WOULD_BLOCK_THREAD;
      if (report_error) {
        std::string message("Blocking callbacks are not allowed while handling "
                            "a blocking message from JavaScript.");
        PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                    std::string(), message);
      }
    } else if (!IsMainThread() &&
               callback_->has_null_target_loop() &&
               !callback_->is_blocking()) {
      // On a non-main thread, there must be a valid target loop for non-
      // blocking callbacks, or we will have no place to run them.

      // If the callback is required, there's no nice way to tell the plugin.
      // We can't run their callback asynchronously without a message loop, and
      // the plugin won't expect any return code other than
      // PP_OK_COMPLETIONPENDING. So we crash to make the problem more obvious.
      if (callback_->is_required()) {
        std::string message("Attempted to use a required callback, but there "
                            "is no attached message loop on which to run the "
                            "callback.");
        PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                    std::string(), message);
        LOG(FATAL) << message;
      }

      callback_->MarkAsCompleted();
      callback_ = nullptr;
      retval_ = PP_ERROR_NO_MESSAGE_LOOP;
      if (report_error) {
        std::string message(
            "The calling thread must have a message loop attached.");
        PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                    std::string(), message);
      }
    }
  }
}

void EnterBase::ClearCallback() {
  callback_ = nullptr;
}

void EnterBase::SetStateForResourceError(PP_Resource pp_resource,
                                         Resource* resource_base,
                                         void* object,
                                         bool report_error) {
  // Check for callback errors. If we get any, SetStateForCallbackError will
  // emit a log message. But we also want to check for resource errors. If there
  // are both kinds of errors, we'll emit two log messages and return
  // PP_ERROR_BADRESOURCE.
  SetStateForCallbackError(report_error);

  if (object)
    return;  // Everything worked.

  if (callback_ && callback_->is_required()) {
    callback_->PostRun(static_cast<int32_t>(PP_ERROR_BADRESOURCE));
    callback_ = nullptr;
    retval_ = PP_OK_COMPLETIONPENDING;
  } else {
    if (callback_)
      callback_->MarkAsCompleted();
    callback_ = nullptr;
    retval_ = PP_ERROR_BADRESOURCE;
  }

  // We choose to silently ignore the error when the pp_resource is null
  // because this is a pretty common case and we don't want to have lots
  // of errors in the log. This should be an obvious case to debug.
  if (report_error && pp_resource) {
    std::string message;
    if (resource_base) {
      message = base::StringPrintf(
          "0x%X is not the correct type for this function.",
          pp_resource);
    } else {
      message = base::StringPrintf(
          "0x%X is not a valid resource ID.",
          pp_resource);
    }
    PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                std::string(), message);
  }
}

void EnterBase::SetStateForFunctionError(PP_Instance pp_instance,
                                         void* object,
                                         bool report_error) {
  // Check for callback errors. If we get any, SetStateForCallbackError will
  // emit a log message. But we also want to check for instance errors. If there
  // are both kinds of errors, we'll emit two log messages and return
  // PP_ERROR_BADARGUMENT.
  SetStateForCallbackError(report_error);

  if (object)
    return;  // Everything worked.

  if (callback_ && callback_->is_required()) {
    callback_->PostRun(static_cast<int32_t>(PP_ERROR_BADARGUMENT));
    callback_ = nullptr;
    retval_ = PP_OK_COMPLETIONPENDING;
  } else {
    if (callback_)
      callback_->MarkAsCompleted();
    callback_ = nullptr;
    retval_ = PP_ERROR_BADARGUMENT;
  }

  // We choose to silently ignore the error when the pp_instance is null as
  // for PP_Resources above.
  if (report_error && pp_instance) {
    std::string message;
    message = base::StringPrintf(
        "0x%X is not a valid instance ID.",
        pp_instance);
    PpapiGlobals::Get()->BroadcastLogWithSource(0, PP_LOGLEVEL_ERROR,
                                                std::string(), message);
  }
}

}  // namespace subtle

EnterInstance::EnterInstance(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstance::EnterInstance(PP_Instance instance,
                             const PP_CompletionCallback& callback)
    : EnterBase(0 /* resource */, callback),
      // TODO(dmichael): This means that the callback_ we get is not associated
      //                 even with the instance, but we should handle that for
      //                 MouseLock (maybe others?).
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstance::~EnterInstance() {
}

EnterInstanceNoLock::EnterInstanceNoLock(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstanceNoLock::EnterInstanceNoLock(
    PP_Instance instance,
    const PP_CompletionCallback& callback)
    : EnterBase(0 /* resource */, callback),
      // TODO(dmichael): This means that the callback_ we get is not associated
      //                 even with the instance, but we should handle that for
      //                 MouseLock (maybe others?).
      functions_(PpapiGlobals::Get()->GetInstanceAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterInstanceNoLock::~EnterInstanceNoLock() {
}

EnterResourceCreation::EnterResourceCreation(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetResourceCreationAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterResourceCreation::~EnterResourceCreation() {
}

EnterResourceCreationNoLock::EnterResourceCreationNoLock(PP_Instance instance)
    : EnterBase(),
      functions_(PpapiGlobals::Get()->GetResourceCreationAPI(instance)) {
  SetStateForFunctionError(instance, functions_, true);
}

EnterResourceCreationNoLock::~EnterResourceCreationNoLock() {
}

}  // namespace thunk
}  // namespace ppapi
