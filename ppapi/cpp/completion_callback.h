// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_CPP_COMPLETION_CALLBACK_H_
#define PPAPI_CPP_COMPLETION_CALLBACK_H_

#include <stdint.h>

#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"         // nogncheck http://crbug.com/1228394
#include "ppapi/cpp/output_traits.h"  // nogncheck http://crbug.com/1228394

/// @file
/// This file defines the API to create and run a callback.
namespace pp {

/// This API enables you to implement and receive callbacks when
/// Pepper operations complete asynchronously.
///
/// You can create these objects yourself, but it is most common to use the
/// CompletionCallbackFactory to allow the callbacks to call class member
/// functions.
class CompletionCallback {
 public:
  /// The default constructor will create a blocking
  /// <code>CompletionCallback</code> that can be passed to a method to
  /// indicate that the calling thread should be blocked until the asynchronous
  /// operation corresponding to the method completes.
  ///
  /// <strong>Note:</strong> Blocking completion callbacks are only allowed from
  /// from background threads.
  CompletionCallback() {
    cc_ = PP_BlockUntilComplete();
  }

  /// A constructor for creating a <code>CompletionCallback</code>.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  CompletionCallback(PP_CompletionCallback_Func func, void* user_data) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
  }

  /// A constructor for creating a <code>CompletionCallback</code> with
  /// specified flags.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  CompletionCallback(PP_CompletionCallback_Func func, void* user_data,
                     int32_t flags) {
    cc_ = PP_MakeCompletionCallback(func, user_data);
    cc_.flags = flags;
  }

  /// The set_flags() function is used to set the flags used to control
  /// how non-NULL callbacks are scheduled by asynchronous methods.
  ///
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  void set_flags(int32_t flags) { cc_.flags = flags; }

  /// Run() is used to run the <code>CompletionCallback</code>.
  /// Normally, the system runs a <code>CompletionCallback</code> after an
  /// asynchronous operation completes, but programs may wish to run the
  /// <code>CompletionCallback</code> manually in order to reuse the same code
  /// paths.
  ///
  /// @param[in] result The result of the operation to be passed to the
  /// callback function. Non-positive values correspond to the error codes
  /// from <code>pp_errors.h</code> (excluding
  /// <code>PP_OK_COMPLETIONPENDING</code>). Positive values indicate
  /// additional information such as bytes read.
  void Run(int32_t result) {
    PP_DCHECK(cc_.func);
    PP_RunCompletionCallback(&cc_, result);
  }

  /// RunAndClear() is used to run the <code>CompletionCallback</code> and
  /// clear out the callback so that it cannot be run a second time.
  ///
  /// @param[in] result The result of the operation to be passed to the
  /// callback function. Non-positive values correspond to the error codes
  /// from <code>pp_errors.h</code> (excluding
  /// <code>PP_OK_COMPLETIONPENDING</code>). Positive values indicate
  /// additional information such as bytes read.
  void RunAndClear(int32_t result) {
    PP_DCHECK(cc_.func);
    PP_RunAndClearCompletionCallback(&cc_, result);
  }

  /// IsOptional() is used to determine the setting of the
  /// <code>PP_COMPLETIONCALLBACK_FLAG_OPTIONAL</code> flag. This flag allows
  /// any method taking such callback to complete synchronously
  /// and not call the callback if the operation would not block. This is useful
  /// when performance is an issue, and the operation bandwidth should not be
  /// limited to the processing speed of the message loop.
  ///
  /// On synchronous method completion, the completion result will be returned
  /// by the method itself. Otherwise, the method will return
  /// PP_OK_COMPLETIONPENDING, and the callback will be invoked asynchronously
  /// on the same thread where the PPB method was invoked.
  ///
  /// @return true if this callback is optional, otherwise false.
  bool IsOptional() const {
    return (cc_.func == NULL ||
            (cc_.flags & PP_COMPLETIONCALLBACK_FLAG_OPTIONAL) != 0);
  }

  /// The pp_completion_callback() function returns the underlying
  /// <code>PP_CompletionCallback</code>
  ///
  /// @return A <code>PP_CompletionCallback</code>.
  const PP_CompletionCallback& pp_completion_callback() const { return cc_; }

  /// The flags() function returns flags used to control how non-NULL callbacks
  /// are scheduled by asynchronous methods.
  ///
  /// @return An int32_t containing a bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags.
  int32_t flags() const { return cc_.flags; }

  /// MayForce() is used when implementing functions taking callbacks.
  /// If the callback is required and <code>result</code> indicates that it has
  /// not been scheduled, it will be forced on the main thread.
  ///
  /// <strong>Example:</strong>
  ///
  /// @code
  ///
  /// int32_t OpenURL(pp::URLLoader* loader,
  ///                 pp::URLRequestInfo* url_request_info,
  ///                 const CompletionCallback& cc) {
  ///   if (loader == NULL || url_request_info == NULL)
  ///     return cc.MayForce(PP_ERROR_BADRESOURCE);
  ///   return loader->Open(*loader, *url_request_info, cc);
  /// }
  ///
  /// @endcode
  ///
  /// @param[in] result PP_OK_COMPLETIONPENDING or the result of the completed
  /// operation to be passed to the callback function. PP_OK_COMPLETIONPENDING
  /// indicates that the callback has already been scheduled. Other
  /// non-positive values correspond to error codes from
  /// <code>pp_errors.h</code>. Positive values indicate additional information
  /// such as bytes read.
  ///
  /// @return <code>PP_OK_COMPLETIONPENDING</code> if the callback has been
  /// forced, result parameter otherwise.
  int32_t MayForce(int32_t result) const {
    if (result == PP_OK_COMPLETIONPENDING || IsOptional())
      return result;
    // FIXME(dmichael): Use pp::MessageLoop here once it's out of Dev.
    Module::Get()->core()->CallOnMainThread(0, *this, result);
    return PP_OK_COMPLETIONPENDING;
  }

 protected:
  PP_CompletionCallback cc_;
};

/// A CompletionCallbackWithOutput defines a completion callback that
/// additionally stores a pointer to some output data. Some C++ wrappers
/// take a CompletionCallbackWithOutput when the browser is returning a
/// bit of data as part of the function call. The "output" parameter
/// stored in the CompletionCallbackWithOutput will receive the data from
/// the browser.
///
/// You can create this yourself, but it is most common to use with the
/// CompletionCallbackFactory's NewCallbackWithOutput, which manages the
/// storage for the output parameter for you and passes it as an argument
/// to your callback function.
///
/// Note that this class doesn't actually do anything with the output data,
/// it just stores a pointer to it. C++ wrapper objects that accept a
/// CompletionCallbackWithOutput will retrieve this pointer and pass it to
/// the browser as the output parameter.
template<typename T>
class CompletionCallbackWithOutput : public CompletionCallback {
 public:
  /// The type that will actually be stored in the completion callback. In the
  /// common case, this will be equal to the template parameter (for example,
  /// CompletionCallbackWithOutput<int> would obviously take an int*. However,
  /// resources are passed as PP_Resource, vars as PP_Var, and arrays as our
  /// special ArrayOutputAdapter object. The CallbackOutputTraits defines
  /// specializations for all of these cases.
  typedef typename internal::CallbackOutputTraits<T>::StorageType
      OutputStorageType;
  typedef typename internal::CallbackOutputTraits<T>::APIArgType
      APIArgType;

  /// The default constructor will create a blocking
  /// <code>CompletionCallback</code> that references the given output
  /// data.
  ///
  /// @param[in] output A pointer to the data associated with the callback. The
  /// caller must ensure that this pointer outlives the completion callback.
  ///
  /// <strong>Note:</strong> Blocking completion callbacks are only allowed from
  /// from background threads.
  CompletionCallbackWithOutput(OutputStorageType* output)
      : CompletionCallback(),
        output_(output) {
  }

  /// A constructor for creating a <code>CompletionCallback</code> that
  /// references the given output data.
  ///
  /// @param[in] func The function to be called on completion.
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  /// @param[in] output A pointer to the data associated with the callback. The
  /// caller must ensure that this pointer outlives the completion callback.
  CompletionCallbackWithOutput(PP_CompletionCallback_Func func,
                               void* user_data,
                               OutputStorageType* output)
      : CompletionCallback(func, user_data),
        output_(output) {
  }

  /// A constructor for creating a <code>CompletionCallback</code> that
  /// references the given output data.
  ///
  /// @param[in] func The function to be called on completion.
  ///
  /// @param[in] user_data The user data to be passed to the callback function.
  /// This is optional and is typically used to help track state in case of
  /// multiple pending callbacks.
  ///
  /// @param[in] flags Bit field combination of
  /// <code>PP_CompletionCallback_Flag</code> flags used to control how
  /// non-NULL callbacks are scheduled by asynchronous methods.
  ///
  /// @param[in] output A pointer to the data associated with the callback. The
  /// caller must ensure that this pointer outlives the completion callback.
  CompletionCallbackWithOutput(PP_CompletionCallback_Func func,
                               void* user_data,
                               int32_t flags,
                               OutputStorageType* output)
      : CompletionCallback(func, user_data, flags),
        output_(output) {
  }

  APIArgType output() const {
    return internal::CallbackOutputTraits<T>::StorageToAPIArg(*output_);
  }

 private:
  OutputStorageType* output_;
};

/// BlockUntilComplete() is used in place of an actual completion callback
/// to request blocking behavior. If specified, the calling thread will block
/// until the function completes. Blocking completion callbacks are only
/// allowed from background threads.
///
/// @return A <code>CompletionCallback</code> corresponding to a NULL callback.
inline CompletionCallback BlockUntilComplete() {
  // Note: Explicitly inlined to avoid link errors when included into
  // ppapi_proxy and ppapi_cpp_objects.
  return CompletionCallback();
}

}  // namespace pp

#endif  // PPAPI_CPP_COMPLETION_CALLBACK_H_
