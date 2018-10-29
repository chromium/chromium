// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_UTILS_H_
#define PPAPI_TESTS_TEST_UTILS_H_

#include <string>

#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
class NetAddress;
class URLLoader;
class URLRequestInfo;
}

// Timeout to wait for some action to complete.
extern const int kActionTimeoutMs;

const PPB_Testing_Private* GetTestingInterface();
std::string ReportError(const char* method, int32_t error);
void PlatformSleep(int duration_ms);

// Returns the host and port of the current document's URL (Which is generally
// served by an EmbeddedTestServer). Returns false on failure.
bool GetLocalHostPort(PP_Instance instance, std::string* host, uint16_t* port);

uint16_t ConvertFromNetEndian16(uint16_t x);
uint16_t ConvertToNetEndian16(uint16_t x);
bool EqualNetAddress(const pp::NetAddress& addr1, const pp::NetAddress& addr2);
// Only returns the first address if there are more than one available.
bool ResolveHost(PP_Instance instance,
                 const std::string& host,
                 uint16_t port,
                 pp::NetAddress* addr);
bool ReplacePort(PP_Instance instance,
                 const pp::NetAddress& input_addr,
                 uint16_t port,
                 pp::NetAddress* output_addr);
uint16_t GetPort(const pp::NetAddress& addr);

// NestedEvent allows you to run a nested MessageLoop and wait for a particular
// event to complete. For example, you can use it to wait for a callback on a
// PPP interface, which will "Signal" the event and make the loop quit.
// "Wait()" will return immediately if it has already been signalled. Otherwise,
// it will run a nested run loop (using PPB_Testing.RunMessageLoop) and will
// return only after it has been signalled.
// Example:
//  std::string TestFullscreen::TestNormalToFullscreen() {
//    pp::Fullscreen screen_mode(instance);
//    screen_mode.SetFullscreen(true);
//    SimulateUserGesture();
//    // Let DidChangeView run in a nested run loop.
//    nested_event_.Wait();
//    Pass();
//  }
//
//  void TestFullscreen::DidChangeView(const pp::View& view) {
//    nested_event_.Signal();
//  }
//
// All methods except Signal and PostSignal must be invoked on the main thread.
// It's OK to signal from a background thread, so you can (for example) Signal()
// from the Audio thread.
class NestedEvent {
 public:
  explicit NestedEvent(PP_Instance instance)
      : instance_(instance), waiting_(false), signalled_(false) {
  }
  // Run a nested run loop and wait until Signal() is called. If Signal()
  // has already been called, return immediately without running a nested loop.
  void Wait();
  // Signal the NestedEvent. If Wait() has been called, quit the message loop.
  // This can be called from any thread.
  void Signal();
  // Signal the NestedEvent in |wait_ms| milliseconds. This can be called from
  // any thread.
  void PostSignal(int32_t wait_ms);

  // Reset the NestedEvent so it can be used again.
  void Reset();

 private:
  void SignalOnMainThread();
  static void SignalThunk(void* async_event, int32_t result);

  PP_Instance instance_;
  bool waiting_;
  bool signalled_;
  // Disable copy and assign.
  NestedEvent(const NestedEvent&);
  NestedEvent& operator=(const NestedEvent&);
};

// Returns a callback that does nothing, so can be invoked when the current
// function is out of scope, unlike TestCompletionCallback.
pp::CompletionCallback DoNothingCallback();

template <typename OutputT>
void DeleteStorage(void* user_data, int32_t flags) {
  typename pp::CompletionCallbackWithOutput<OutputT>::OutputStorageType*
      storage = reinterpret_cast<typename pp::CompletionCallbackWithOutput<
          OutputT>::OutputStorageType*>(user_data);
  delete storage;
}

// Same as DoNothingCallback(), but with an OutputStorageType, which it deletes
// when the callback is invoked.
template <typename OutputT>
pp::CompletionCallbackWithOutput<OutputT> DoNothingCallbackWithOutput() {
  typename pp::CompletionCallbackWithOutput<OutputT>::OutputStorageType*
      storage = new
      typename pp::CompletionCallbackWithOutput<OutputT>::OutputStorageType();
  return pp::CompletionCallbackWithOutput<OutputT>(
      &DeleteStorage<OutputT>, storage, PP_COMPLETIONCALLBACK_FLAG_OPTIONAL,
      storage);
}

enum CallbackType { PP_REQUIRED, PP_OPTIONAL, PP_BLOCKING };
class TestCompletionCallback {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual void OnCallback(void* user_data, int32_t result) = 0;
  };
  explicit TestCompletionCallback(PP_Instance instance);
  // TODO(dmichael): Remove this constructor.
  TestCompletionCallback(PP_Instance instance, bool force_async);

  TestCompletionCallback(PP_Instance instance, CallbackType callback_type);

  // Sets a Delegate instance. OnCallback() of this instance will be invoked
  // when the completion callback is invoked.
  // The delegate will be reset when Reset() or GetCallback() is called.
  void SetDelegate(Delegate* delegate) { delegate_ = delegate; }

  // Wait for a result, given the return from the call which took this callback
  // as a parameter. If |result| is PP_OK_COMPLETIONPENDING, WaitForResult will
  // block until its callback has been invoked (in some cases, this will already
  // have happened, and WaitForCallback can return immediately).
  // For any other values, WaitForResult will simply set its internal "result_"
  // field. To retrieve the final result of the operation (i.e., the result
  // the callback has run, if necessary), call result(). You can call result()
  // as many times as necessary until a new pp::CompletionCallback is retrieved.
  //
  // In some cases, you may want to check that the callback was invoked in the
  // expected way (i.e., if the callback was "Required", then it should be
  // invoked asynchronously). Within the body of a test (where returning a non-
  // empty string indicates test failure), you can use the
  // CHECK_CALLBACK_BEHAVIOR(callback) macro. From within a helper function,
  // you can use failed() and errors().
  //
  // Example usage within a test:
  //  callback.WaitForResult(foo.DoSomething(callback));
  //  CHECK_CALLBACK_BEHAVIOR(callback);
  //  ASSERT_EQ(PP_OK, callback.result());
  //
  // Example usage within a helper function:
  //  void HelperFunction(std::string* error_message) {
  //    callback.WaitForResult(foo.DoSomething(callback));
  //    if (callback.failed())
  //      error_message->assign(callback.errors());
  //  }
  void WaitForResult(int32_t result);

  // Used when you expect to receive either synchronous completion with PP_OK
  // or a PP_ERROR_ABORTED asynchronously.
  //  Example usage:
  //  int32_t result = 0;
  //  {
  //    pp::URLLoader temp(instance_);
  //    result = temp.Open(request, callback);
  //  }
  //  callback.WaitForAbortResult(result);
  //  CHECK_CALLBACK_BEHAVIOR(callback);
  void WaitForAbortResult(int32_t result);

  // Retrieve a pp::CompletionCallback for use in testing. This Reset()s the
  // TestCompletionCallback.
  pp::CompletionCallback GetCallback();

  bool failed() { return !errors_.empty(); }
  const std::string& errors() { return errors_; }

  int32_t result() const { return result_; }

  // Reset so that this callback can be used again.
  void Reset();

  CallbackType callback_type() { return callback_type_; }
  void set_target_loop(const pp::MessageLoop& loop) { target_loop_ = loop; }
  static void Handler(void* user_data, int32_t result);

 protected:
  void RunMessageLoop();
  void QuitMessageLoop();

  // Used to check that WaitForResult is only called once for each usage of the
  // callback.
  bool wait_for_result_called_;
  // Indicates whether we have already been invoked.
  bool have_result_;
  // The last result received (or PP_OK_COMPLETIONCALLBACK if none).
  int32_t result_;
  CallbackType callback_type_;
  bool post_quit_task_;
  std::string errors_;
  PP_Instance instance_;
  Delegate* delegate_;
  pp::MessageLoop target_loop_;
};

template <typename OutputT>
class TestCompletionCallbackWithOutput {
 public:
  explicit TestCompletionCallbackWithOutput(PP_Instance instance)
      : callback_(instance),
        output_storage_() {
    pp::internal::CallbackOutputTraits<OutputT>::Initialize(&output_storage_);
  }

  TestCompletionCallbackWithOutput(PP_Instance instance, bool force_async)
      : callback_(instance, force_async),
        output_storage_() {
    pp::internal::CallbackOutputTraits<OutputT>::Initialize(&output_storage_);
  }

  TestCompletionCallbackWithOutput(PP_Instance instance,
                                   CallbackType callback_type)
      : callback_(instance, callback_type),
        output_storage_() {
    pp::internal::CallbackOutputTraits<OutputT>::Initialize(&output_storage_);
  }

  pp::CompletionCallbackWithOutput<OutputT> GetCallback();
  OutputT output() {
    return pp::internal::CallbackOutputTraits<OutputT>::StorageToPluginArg(
        output_storage_);
  }

  // Delegate functions to TestCompletionCallback
  void SetDelegate(TestCompletionCallback::Delegate* delegate) {
    callback_.SetDelegate(delegate);
  }
  void WaitForResult(int32_t result) { callback_.WaitForResult(result); }
  void WaitForAbortResult(int32_t result) {
    callback_.WaitForAbortResult(result);
  }
  bool failed() { return callback_.failed(); }
  const std::string& errors() { return callback_.errors(); }
  int32_t result() const { return callback_.result(); }
  void Reset() {
    pp::internal::CallbackOutputTraits<OutputT>::Initialize(&output_storage_);
    return callback_.Reset();
  }

 private:
  TestCompletionCallback callback_;
  typename pp::CompletionCallbackWithOutput<OutputT>::OutputStorageType
      output_storage_;
};

template <typename OutputT>
pp::CompletionCallbackWithOutput<OutputT>
TestCompletionCallbackWithOutput<OutputT>::GetCallback() {
  this->Reset();
  if (callback_.callback_type() == PP_BLOCKING) {
    pp::CompletionCallbackWithOutput<OutputT> cc(&output_storage_);
    return cc;
  }

  callback_.set_target_loop(pp::MessageLoop::GetCurrent());
  pp::CompletionCallbackWithOutput<OutputT> cc(
      &TestCompletionCallback::Handler,
      this,
      &output_storage_);
  if (callback_.callback_type() == PP_OPTIONAL)
    cc.set_flags(PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
  return cc;
}

// Verifies that the callback didn't record any errors. If the callback is run
// in an unexpected way (e.g., if it's invoked asynchronously when the call
// should have blocked), this returns an appropriate error string.
#define CHECK_CALLBACK_BEHAVIOR(callback) \
do { \
  if ((callback).failed()) \
    return MakeFailureMessage(__FILE__, __LINE__, \
                              (callback).errors().c_str()); \
} while (false)

/*
 * A set of macros to use for platform detection. These were largely copied
 * from chromium's build_config.h.
 */
#if defined(__APPLE__)
#define PPAPI_OS_MACOSX 1
#elif defined(ANDROID)
#define PPAPI_OS_ANDROID 1
#elif defined(__native_client__)
#define PPAPI_OS_NACL 1
#elif defined(__linux__)
#define PPAPI_OS_LINUX 1
#elif defined(_WIN32)
#define PPAPI_OS_WIN 1
#elif defined(__FreeBSD__)
#define PPAPI_OS_FREEBSD 1
#elif defined(__OpenBSD__)
#define PPAPI_OS_OPENBSD 1
#elif defined(__sun)
#define PPAPI_OS_SOLARIS 1
#else
#error Please add support for your platform in ppapi/tests/test_utils.h
#endif

/* These are used to determine POSIX-like implementations vs Windows. */
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) || \
    defined(__OpenBSD__) || defined(__sun) || defined(__native_client__)
#define PPAPI_POSIX 1
#endif

// By default, ArrayBuffers over a certain size are sent via shared memory. In
// order to test for this without sending huge buffers, tests can use this
// class to set the minimum array buffer size used for shared memory temporarily
// lower.
class ScopedArrayBufferSizeSetter {
 public:
  ScopedArrayBufferSizeSetter(const PPB_Testing_Private* interface,
                              PP_Instance instance,
                              uint32_t threshold)
     : interface_(interface),
       instance_(instance) {
    interface_->SetMinimumArrayBufferSizeForShmem(instance_, threshold);
  }
  ~ScopedArrayBufferSizeSetter() {
    interface_->SetMinimumArrayBufferSizeForShmem(instance_, 0);
  }

 private:
  const PPB_Testing_Private* interface_;
  PP_Instance instance_;
};

// Opens |request| in |loader| and returns the results of the URLRequest.  The
// caller may provide the optional |response_body| argument to get the contents
// of the body of the response to the URLRequest.
//
// Returns PP_OK upon success.
int32_t OpenURLRequest(PP_Instance instance,
                       pp::URLLoader* loader,
                       const pp::URLRequestInfo& request,
                       CallbackType callback_type,
                       std::string* response_body);

#endif  // PPAPI_TESTS_TEST_UTILS_H_
