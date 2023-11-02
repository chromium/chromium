// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_MESSAGE_LOOP_H_
#define PPAPI_CPP_MESSAGE_LOOP_H_

#include <stdint.h>

#include "ppapi/cpp/resource.h"

/// @file
/// This file defines the PPB_MessageLoop API.

namespace pp {

class CompletionCallback;
class InstanceHandle;

/// A message loop allows PPAPI calls to be issued on a thread. You may not
/// issue any API calls on a thread without creating a message loop. It also
/// allows you to post work to the message loop for a thread.
///
/// To process work posted to the message loop, as well as completion callbacks
/// for asynchronous operations, you must run the message loop via Run().
///
/// Note the system manages the lifetime of the instance (and all associated
/// resources). If the instance is deleted from the page, background threads may
/// suddenly see their PP_Resource handles become invalid. In this case, calls
/// will fail with PP_ERROR_BADRESOURCE. If you need to access data associated
/// with your instance, you will probably want to create some kind of threadsafe
/// proxy object that can handle asynchronous destruction of the instance
/// object.
///
/// Typical usage:
///   On the main thread:
///    - Create the thread yourself (using pthreads).
///    - Create the message loop resource.
///    - Pass the message loop resource to your thread's main function.
///    - Call PostWork() on the message loop to run functions on the thread.
///
///   From the background thread's main function:
///    - Call AttachToCurrentThread() with the message loop resource.
///    - Call Run() with the message loop resource.
///
///   Your callbacks should look like this:
///   @code
///   void DoMyWork(void* user_data, int32_t status) {
///     if (status != PP_OK) {
///       Cleanup();  // e.g. free user_data.
///       return;
///     }
///     ... do your work...
///   }
///   @endcode
/// For a C++ example, see ppapi/utility/threading/simple_thread.h
///
/// (You can also create the message loop resource on the background thread,
/// but then the main thread will have no reference to it should you want to
/// call PostWork()).
///
///
/// THREAD HANDLING
///
/// The main thread has an implicitly created message loop. The main thread is
/// the thread where PPP_InitializeModule and PPP_Instance functions are called.
/// You can retrieve a reference to this message loop by calling
/// GetForMainThread() or, if your code is on the main thread, GetCurrent() will
/// also work.
///
/// Some special threads created by the system can not have message loops. In
/// particular, the background thread created for audio processing has this
/// requirement because it's intended to be highly responsive to keep up with
/// the realtime requirements of audio processing. You can not make PPAPI calls
/// from these threads.
///
/// Once you associate a message loop with a thread, you don't have to keep a
/// reference to it. The system will hold a reference to the message loop for as
/// long as the thread is running. The current message loop can be retrieved
/// using the GetCurrent() function.
///
/// It is legal to create threads in your plugin without message loops, but
/// PPAPI calls will fail unless explicitly noted in the documentation.
///
/// You can create a message loop object on a thread and never actually run the
/// message loop. This will allow you to call blocking PPAPI calls (via
/// PP_BlockUntilComplete()). If you make any asynchronous calls, the callbacks
/// from those calls will be queued in the message loop and never run. The same
/// thing will happen if work is scheduled after the message loop exits and
/// the message loop is not run again.
///
///
/// DESTRUCTION AND ERROR HANDLING
///
/// Often, your application will associate memory with completion callbacks. For
/// example, the C++ CompletionCallbackFactory has a small amount of
/// heap-allocated memory for each callback. This memory will be leaked if the
/// callback is never run. To avoid this memory leak, you need to be careful
/// about error handling and shutdown.
///
/// There are a number of cases where posted callbacks will never be run:
///
///  - You tear down the thread (via pthreads) without "destroying" the message
///    loop (via PostQuit with should_destroy = PP_TRUE). In this case, any
///    tasks in the message queue will be lost.
///
///  - You create a message loop, post callbacks to it, and never run it.
///
///  - You quit the message loop via PostQuit with should_destroy set to
///    PP_FALSE. In this case, the system will assume the message loop will be
///    run again later and keep your tasks.
///
/// To do proper shutdown, call PostQuit with should_destroy = PP_TRUE. This
/// will prohibit future work from being posted, and will allow the message loop
/// to run until all pending tasks are run.
///
/// If you post a callback to a message loop that's been destroyed, or to an
/// invalid message loop, PostWork will return an error and will not run the
/// callback. This is true even for callbacks with the "required" flag set,
/// since the system may not even know what thread to issue the error callback
/// on.
///
/// Therefore, you should check for errors from PostWork and destroy any
/// associated memory to avoid leaks. If you're using the C++
/// CompletionCallbackFactory, use the following pattern:
/// @code
/// pp::CompletionCallback callback = factory_.NewOptionalCallback(...);
/// int32_t result = message_loop.PostWork(callback);
/// if (result != PP_OK)
///   callback.Run(result);
/// @endcode
/// This will run the callback with an error value, and assumes that the
/// implementation of your callback checks the "result" argument and returns
/// immediately on error.
class MessageLoop : public Resource {
 public:
  /// Creates an is_null() MessageLoop resource.
  MessageLoop();

  /// Creates a message loop associated with the given instance. The resource
  /// will be is_null() on failure.
  ///
  /// This may be called from any thread. After your thread starts but before
  /// issuing any other PPAPI calls on it, you must associate it with a message
  /// loop by calling AttachToCurrentThread.
  explicit MessageLoop(const InstanceHandle& instance);

  MessageLoop(const MessageLoop& other);
  MessageLoop& operator=(const MessageLoop& other);

  /// Takes an additional ref to the resource.
  explicit MessageLoop(PP_Resource pp_message_loop);

  static MessageLoop GetForMainThread();
  static MessageLoop GetCurrent();

  /// Sets the given message loop resource as being the associated message loop
  /// for the currently running thread.
  ///
  /// You must call this function exactly once on a thread before making any
  /// PPAPI calls. A message loop can only be attached to one thread, and the
  /// message loop can not be changed later. The message loop will be attached
  /// as long as the thread is running or until you quit with should_destroy
  /// set to PP_TRUE.
  ///
  /// If this function fails, attempting to run the message loop will fail.
  /// Note that you can still post work to the message loop: it will get queued
  /// up should the message loop eventually be successfully attached and run.
  ///
  /// @return
  ///   - PP_OK: The message loop was successfully attached to the thread and is
  ///     ready to use.
  ///   - PP_ERROR_BADRESOURCE: The given message loop resource is invalid.
  ///   - PP_ERROR_INPROGRESS: The current thread already has a message loop
  ///     attached. This will always be the case for the main thread, which has
  ///     an implicit system-created message loop attached.
  ///   - PP_ERROR_WRONG_THREAD: The current thread type can not have a message
  ///     loop attached to it. See the interface level discussion about these
  ///     special threads, which include realtime audio threads.
  int32_t AttachToCurrentThread();

  /// Runs the thread message loop. Running the message loop is required for
  /// you to get issued completion callbacks on the thread.
  ///
  /// The message loop identified by the argument must have been previously
  /// successfully attached to the current thread.
  ///
  /// You may not run nested run loops. Since the main thread has an
  /// implicit message loop that the system runs, you may not call Run on the
  /// main thread.
  ///
  /// @return
  ///   - PP_OK: The message loop was successfully run. Note that on
  ///     success, the message loop will only exit when you call PostQuit().
  ///   - PP_ERROR_BADRESOURCE: The given message loop resource is invalid.
  ///   - PP_ERROR_WRONG_THREAD: You are attempting to run a message loop that
  ///     has not been successfully attached to the current thread. Call
  ///     AttachToCurrentThread().
  ///   - PP_ERROR_INPROGRESS: You are attempting to call Run in a nested
  ///     fashion (Run is already on the stack). This will occur if you attempt
  ///     to call run on the main thread's message loop (see above).
  int32_t Run();

  /// Schedules work to run on the given message loop. This may be called from
  /// any thread. Posted work will be executed in the order it was posted when
  /// the message loop is Run().
  ///
  /// @param callback A pointer to the completion callback to execute from the
  /// message loop.
  ///
  /// @param delay_ms The number of milliseconds to delay execution of the given
  /// completion callback. Passing 0 means it will get queued normally and
  /// executed in order.
  ///
  ///
  /// The completion callback will be called with PP_OK as the "result"
  /// parameter if it is run normally. It is good practice to check for PP_OK
  /// and return early otherwise.
  ///
  /// The "required" flag on the completion callback is ignored. If there is an
  /// error posting your callback, the error will be returned from PostWork and
  /// the callback will never be run (because there is no appropriate place to
  /// run your callback with an error without causing unexpected threading
  /// problems). If you associate memory with the completion callback (for
  /// example, you're using the C++ CompletionCallbackFactory), you will need to
  /// free this or manually run the callback. See "Desctruction and error
  /// handling" above.
  ///
  ///
  /// You can call this function before the message loop has started and the
  /// work will get queued until the message loop is run. You can also post
  /// work after the message loop has exited as long as should_destroy was
  /// PP_FALSE. It will be queued until the next invocation of Run().
  ///
  /// @return
  ///   - PP_OK: The work was posted to the message loop's queue. As described
  ///     above, this does not mean that the work has been or will be executed
  ///     (if you never run the message loop after posting).
  ///   - PP_ERROR_BADRESOURCE: The given message loop resource is invalid.
  ///   - PP_ERROR_BADARGUMENT: The function pointer for the completion callback
  ///     is null (this will be the case if you pass PP_BlockUntilComplete()).
  ///   - PP_ERROR_FAILED: The message loop has been destroyed.
  int32_t PostWork(const CompletionCallback& callback,
                   int64_t delay_ms = 0);

  /// Posts a quit message to the given message loop's work queue. Work posted
  /// before that point will be processed before quitting.
  ///
  /// This may be called on the message loop registered for the current thread,
  /// or it may be called on the message loop registered for another thread. It
  /// is an error to attempt to quit the main thread loop.
  ///
  /// @param should_destroy Marks the message loop as being in a destroyed
  /// state and prevents further posting of messages.
  ///
  /// If you quit a message loop without setting should_destroy, it will still
  /// be attached to the thread and you can still run it again by calling Run()
  /// again. If you destroy it, it will be detached from the current thread.
  ///
  /// @return
  ///   - PP_OK: The request to quit was successfully posted.
  ///   - PP_ERROR_BADRESOURCE: The message loop was invalid.
  ///   - PP_ERROR_WRONG_THREAD: You are attempting to quit the main thread.
  ///     The main thread's message loop is managed by the system and can't be
  ///     quit.
  int32_t PostQuit(bool should_destroy);
};

}  // namespace pp

#endif  // PPAPI_CPP_MESSAGE_LOOP_H_
