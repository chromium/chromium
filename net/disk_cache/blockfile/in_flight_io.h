// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_DISK_CACHE_BLOCKFILE_IN_FLIGHT_IO_H_
#define NET_DISK_CACHE_BLOCKFILE_IN_FLIGHT_IO_H_

#include <set>

#include "base/check.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace disk_cache {

class InFlightIO;

// This class represents a single asynchronous IO operation while it is being
// bounced between threads.
class BackgroundIO : public base::RefCountedThreadSafe<BackgroundIO> {
 public:
  // Other than the actual parameters for the IO operation (including the
  // |callback| that must be notified at the end), we need the controller that
  // is keeping track of all operations. When done, we notify the controller
  // (we do NOT invoke the callback), in the worker thead that completed the
  // operation.
  explicit BackgroundIO(InFlightIO* controller);

  BackgroundIO(const BackgroundIO&) = delete;
  BackgroundIO& operator=(const BackgroundIO&) = delete;

  // This method signals the controller that this operation is finished, in the
  // original thread. In practice, this is a RunableMethod that allows
  // cancellation.
  void OnIOSignalled();

  // Allows the cancellation of the task to notify the controller (step number 8
  // in the diagram below). In practice, if the controller waits for the
  // operation to finish it doesn't have to wait for the final task to be
  // processed by the message loop so calling this method prevents its delivery.
  // Note that this method is not intended to cancel the actual IO operation or
  // to prevent the first notification to take place (OnIOComplete).
  void Cancel();

  int result() { return result_; }

  base::WaitableEvent* io_completed() {
    return &io_completed_;
  }

 protected:
  virtual ~BackgroundIO();

  // Notifies the controller about the end of the operation, from the background
  // thread.
  void NotifyController();

  // Clears the controller before it might get destroyed.
  void ClearController();

  int result_ = -1;  // Final operation result.

  bool did_notify_controller_io_signalled() const {
    return did_notify_controller_io_signalled_;
  }

 private:
  friend class base::RefCountedThreadSafe<BackgroundIO>;

  // An event to signal when the operation completes.
  base::WaitableEvent io_completed_;
  raw_ptr<InFlightIO>
      controller_;              // The controller that tracks all operations.
  base::Lock controller_lock_;  // A lock protecting clearing of controller_.
  // Set to true if OnIOSignalled() is called *and* the `controller_` was
  // called.
  bool did_notify_controller_io_signalled_ = false;
};

// This class keeps track of asynchronous IO operations. A single instance
// of this class is meant to be used to start an asynchronous operation (using
// PostXX, exposed by a derived class). This class will post the operation to a
// worker thread, hanlde the notification when the operation finishes and
// perform the callback on the same thread that was used to start the operation.
//
// The regular sequence of calls is:
//                 Thread_1                          Worker_thread
//    1.     DerivedInFlightIO::PostXX()
//    2.                         -> PostTask ->
//    3.    InFlightIO::OnOperationPosted()
//    4.                                        DerivedBackgroundIO::XX()
//    5.                                         IO operation completes
//    6.                                       InFlightIO::OnIOComplete()
//    7.                         <- PostTask <-
//    8.  BackgroundIO::OnIOSignalled()
//    9.  InFlightIO::InvokeCallback()
//   10. DerivedInFlightIO::OnOperationComplete()
//   11.       invoke callback
//
// Shutdown is a special case that is handled though WaitForPendingIO() instead
// of just waiting for step 7.
class InFlightIO {
 public:
  InFlightIO();

  InFlightIO(const InFlightIO&) = delete;
  InFlightIO& operator=(const InFlightIO&) = delete;

  virtual ~InFlightIO();

  // Blocks the current thread until all IO operations tracked by this object
  // complete.
  void WaitForPendingIO();

  // Drops current pending operations without waiting for them to complete.
  void DropPendingIO();

  // Called on a background thread when |operation| completes.
  void OnIOComplete(BackgroundIO* operation);

  // Invokes the users' completion callback at the end of the IO operation.
  // |cancel_task| is true if the actual task posted to the thread is still
  // queued (because we are inside WaitForPendingIO), and false if said task is
  // the one performing the call.
  void InvokeCallback(BackgroundIO* operation, bool cancel_task);

 protected:
  // This method is called to signal the completion of the |operation|. |cancel|
  // is true if the operation is being cancelled. This method is called on the
  // thread that created this object.
  virtual void OnOperationComplete(BackgroundIO* operation, bool cancel) = 0;

  // Signals this object that the derived class just posted the |operation| to
  // be executed on a background thread. This method must be called on the same
  // thread used to create this object.
  void OnOperationPosted(BackgroundIO* operation);

 private:
  typedef std::set<scoped_refptr<BackgroundIO> > IOList;

  IOList io_list_;  // List of pending, in-flight io operations.
  scoped_refptr<base::SequencedTaskRunner> callback_task_runner_;

  bool running_ = false;  // True after the first posted operation completes.
#if DCHECK_IS_ON()
  bool single_thread_ = false;  // True if we only have one thread.
#endif
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_BLOCKFILE_IN_FLIGHT_IO_H_
