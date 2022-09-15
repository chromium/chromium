// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_H_
#define PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_H_

#include <stdint.h>

#include "ppapi/cpp/completion_callback.h"
#include "ppapi/utility/completion_callback_factory_thread_traits.h"  // nogncheck http://crbug.com/1228394

/// @file
/// This file defines the API to create CompletionCallback objects that are
/// bound to member functions.
namespace pp {

// TypeUnwrapper --------------------------------------------------------------

namespace internal {

// The TypeUnwrapper converts references and const references to the
// underlying type used for storage and passing as an argument. It is for
// internal use only.
template <typename T> struct TypeUnwrapper {
  typedef T StorageType;
};
template <typename T> struct TypeUnwrapper<T&> {
  typedef T StorageType;
};
template <typename T> struct TypeUnwrapper<const T&> {
  typedef T StorageType;
};

}  // namespace internal

// ----------------------------------------------------------------------------

/// CompletionCallbackFactory<T> may be used to create CompletionCallback
/// objects that are bound to member functions.
///
/// If a factory is destroyed, then any pending callbacks will be cancelled
/// preventing any bound member functions from being called.  The CancelAll()
/// method allows pending callbacks to be cancelled without destroying the
/// factory.
///
/// <strong>Note: </strong><code>CompletionCallbackFactory<T></code> isn't
/// thread safe, but it is somewhat thread-friendly when used with a
/// thread-safe traits class as the second template element. However, it
/// only guarantees safety for creating a callback from another thread, the
/// callback itself needs to execute on the same thread as the thread that
/// creates/destroys the factory. With this restriction, it is safe to create
/// the <code>CompletionCallbackFactory</code> on the main thread, create
/// callbacks from any thread and pass them to CallOnMainThread().
///
/// <strong>Example: </strong>
///
/// @code
///   class MyClass {
///    public:
///     // If an compiler warns on following using |this| in the initializer
///     // list, use PP_ALLOW_THIS_IN_INITIALIZER_LIST macro.
///     MyClass() : factory_(this) {
///     }
///
///     void OpenFile(const pp::FileRef& file) {
///       pp::CompletionCallback cc = factory_.NewCallback(&MyClass::DidOpen);
///       int32_t rv = file_io_.Open(file, PP_FileOpenFlag_Read, cc);
///       CHECK(rv == PP_OK_COMPLETIONPENDING);
///     }
///
///    private:
///     void DidOpen(int32_t result) {
///       if (result == PP_OK) {
///         // The file is open, and we can begin reading.
///         // ...
///       } else {
///         // Failed to open the file with error given by 'result'.
///       }
///     }
///
///     pp::CompletionCallbackFactory<MyClass> factory_;
///   };
/// @endcode
///
/// <strong>Passing additional parameters to your callback</strong>
///
/// As a convenience, the <code>CompletionCallbackFactory</code> can optionally
/// create a closure with up to three bound parameters that it will pass to
/// your callback function. This can be useful for passing information about
/// the request to your callback function, which is especially useful if your
/// class has multiple asynchronous callbacks pending.
///
/// For the above example, of opening a file, let's say you want to keep some
/// description associated with your request, you might implement your OpenFile
/// and DidOpen callback as follows:
///
/// @code
///   void OpenFile(const pp::FileRef& file) {
///     std::string message = "Opening file!";
///     pp::CompletionCallback cc = factory_.NewCallback(&MyClass::DidOpen,
///                                                      message);
///     int32_t rv = file_io_.Open(file, PP_FileOpenFlag_Read, cc);
///     CHECK(rv == PP_OK_COMPLETIONPENDING);
///   }
///   void DidOpen(int32_t result, const std::string& message) {
///     // "message" will be "Opening file!".
///     ...
///   }
/// @endcode
///
/// <strong>Optional versus required callbacks</strong>
///
/// When you create an "optional" callback, the browser may return the results
/// synchronously if they are available. This can allow for higher performance
/// in some cases if data is available quickly (for example, for network loads
/// where there may be a lot of data coming quickly). In this case, the
/// callback will never be run.
///
/// When creating a new callback with the factory, there will be data allocated
/// on the heap that tracks the callback information and any bound arguments.
/// This data is freed when the callback executes. In the case of optional
/// callbacks, since the browser will never issue the callback, the internal
/// tracking data will be leaked.
///
/// Therefore, if you use optional callbacks, it's important to manually
/// issue the callback to free up this data. The typical pattern is:
///
/// @code
///   pp::CompletionCallback callback = callback_factory.NewOptionalCallback(
///       &MyClass::OnDataReady);
///   int32_t result = interface->GetData(callback);
///   if (result != PP_OK_COMPLETIONPENDING)
///      callback.Run(result);
/// @endcode
///
/// Because of this additional complexity, it's generally recommended that
/// you not use optional callbacks except when performance is more important
/// (such as loading large resources from the network). In most other cases,
/// the performance difference will not be worth the additional complexity,
/// and most functions may never actually have the ability to complete
/// synchronously.
///
/// <strong>Completion callbacks with output</strong>
///
/// For some API calls, the browser returns data to the caller via an output
/// parameter. These can be difficult to manage since the output parameter
/// must remain valid for as long as the callback is pending. Note also that
/// CancelAll (or destroying the callback factory) does <i>not</i> cancel the
/// callback from the browser's perspective, only the execution of the callback
/// in the plugin code, and the output parameter will still be written to!
/// This means that you can't use class members as output parameters without
/// risking crashes.
///
/// To make this case easier, the CompletionCallbackFactory can allocate and
/// manage the output data for you and pass it to your callback function. This
/// makes such calls more natural and less error-prone.
///
/// To create such a callback, use NewCallbackWithOutput and specify a callback
/// function that takes the output parameter as its second argument. Let's say
/// you're calling a function GetFile which asynchronously returns a
/// pp::FileRef. GetFile's signature will be <code>int32_t GetFile(const
/// CompletionCallbackWithOutput<pp::FileRef>& callback);</code> and your
/// calling code would look like this:
///
/// @code
///   void RequestFile() {
///     file_interface->GetFile(callback_factory_.NewCallbackWithOutput(
///         &MyClass::GotFile));
///   }
///   void GotFile(int32_t result, const pp::FileRef& file) {
///     if (result == PP_OK) {
///       ...use file...
///     } else {
///       ...handle error...
///     }
///   }
/// @endcode
///
/// As with regular completion callbacks, you can optionally add up to three
/// bound arguments. These are passed following the output argument.
///
/// Your callback may take the output argument as a copy (common for small
/// types like integers, a const reference (common for structures and
/// resources to avoid an extra copy), or as a non-const reference. One
/// optimization you can do if your callback function may take large arrays
/// is to accept your output argument as a non-const reference and to swap()
/// the argument with a vector of your own to store it. This means you don't
/// have to copy the buffer to consume it.
template <typename T, typename ThreadTraits = ThreadSafeThreadTraits>
class CompletionCallbackFactory {
 public:

  /// This constructor creates a <code>CompletionCallbackFactory</code>
  /// bound to an object. If the constructor is called without an argument,
  /// the default value of <code>NULL</code> is used. The user then must call
  /// Initialize() to initialize the object.
  ///
  /// param[in] object Optional parameter. An object whose member functions
  /// are to be bound to CompletionCallbacks created by this
  /// <code>CompletionCallbackFactory</code>. The default value of this
  /// parameter is <code>NULL</code>.
  explicit CompletionCallbackFactory(T* object = NULL)
      : object_(object) {
    // Assume that we don't need to lock since construction should be complete
    // before the pointer is used on another thread.
    InitBackPointer();
  }

  /// Destructor.
  ~CompletionCallbackFactory() {
    // Assume that we don't need to lock since this object should not be used
    // from multiple threads during destruction.
    ResetBackPointer();
  }

  /// CancelAll() cancels all <code>CompletionCallbacks</code> allocated from
  /// this factory.
  void CancelAll() {
    typename ThreadTraits::AutoLock lock(lock_);

    ResetBackPointer();
    InitBackPointer();
  }

  /// Initialize() binds the <code>CallbackFactory</code> to a particular
  /// object. Use this when the object is not available at
  /// <code>CallbackFactory</code> creation, and the <code>NULL</code> default
  /// is passed to the constructor. The object may only be initialized once,
  /// either by the constructor, or by a call to Initialize().
  ///
  /// This class may not be used on any thread until initialization is complete.
  ///
  /// @param[in] object The object whose member functions are to be bound to
  /// the <code>CompletionCallback</code> created by this
  /// <code>CompletionCallbackFactory</code>.
  void Initialize(T* object) {
    PP_DCHECK(object);
    PP_DCHECK(!object_);  // May only initialize once!
    object_ = object;
  }

  /// GetObject() returns the object that was passed at initialization to
  /// Intialize().
  ///
  /// @return the object passed to the constructor or Intialize().
  T* GetObject() {
    return object_;
  }

  /// NewCallback allocates a new, single-use <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method>
  CompletionCallback NewCallback(Method method) {
    return NewCallbackHelper(new Dispatcher0<Method>(method));
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method>
  CompletionCallback NewOptionalCallback(Method method) {
    CompletionCallback cc = NewCallback(method);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewCallbackWithOutput() allocates a new, single-use
  /// <code>CompletionCallback</code> where the browser will pass an additional
  /// parameter containing the result of the request. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Output>
  CompletionCallbackWithOutput<
      typename internal::TypeUnwrapper<Output>::StorageType>
  NewCallbackWithOutput(void (T::*method)(int32_t, Output)) {
    return NewCallbackWithOutputHelper(new DispatcherWithOutput0<
        typename internal::TypeUnwrapper<Output>::StorageType,
        void (T::*)(int32_t, Output)>(method));
  }

  /// NewCallback() allocates a new, single-use <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A>
  CompletionCallback NewCallback(Method method, const A& a) {
    return NewCallbackHelper(new Dispatcher1<Method, A>(method, a));
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A>
  CompletionCallback NewOptionalCallback(Method method, const A& a) {
    CompletionCallback cc = NewCallback(method, a);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewCallbackWithOutput() allocates a new, single-use
  /// <code>CompletionCallback</code> where the browser will pass an additional
  /// parameter containing the result of the request. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Output, typename A>
  CompletionCallbackWithOutput<
      typename internal::TypeUnwrapper<Output>::StorageType>
  NewCallbackWithOutput(void (T::*method)(int32_t, Output, A),
                        const A& a) {
    return NewCallbackWithOutputHelper(new DispatcherWithOutput1<
        typename internal::TypeUnwrapper<Output>::StorageType,
        void (T::*)(int32_t, Output, A),
        typename internal::TypeUnwrapper<A>::StorageType>(method, a));
  }

  /// NewCallback() allocates a new, single-use
  /// <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param method The method taking the callback. Method should be of type:
  /// <code>void (T::*)(int32_t result, const A& a, const B& b)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B>
  CompletionCallback NewCallback(Method method, const A& a, const B& b) {
    return NewCallbackHelper(new Dispatcher2<Method, A, B>(method, a, b));
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method taking the callback. Method should be of
  /// type:
  /// <code>void (T::*)(int32_t result, const A& a, const B& b)</code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B>
  CompletionCallback NewOptionalCallback(Method method, const A& a,
                                         const B& b) {
    CompletionCallback cc = NewCallback(method, a, b);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewCallbackWithOutput() allocates a new, single-use
  /// <code>CompletionCallback</code> where the browser will pass an additional
  /// parameter containing the result of the request. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param[in] method The method to be invoked upon completion of the
  /// operation.
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Output, typename A, typename B>
  CompletionCallbackWithOutput<
      typename internal::TypeUnwrapper<Output>::StorageType>
  NewCallbackWithOutput(void (T::*method)(int32_t, Output, A, B),
                        const A& a,
                        const B& b) {
    return NewCallbackWithOutputHelper(new DispatcherWithOutput2<
        typename internal::TypeUnwrapper<Output>::StorageType,
        void (T::*)(int32_t, Output, A, B),
        typename internal::TypeUnwrapper<A>::StorageType,
        typename internal::TypeUnwrapper<B>::StorageType>(method, a, b));
  }

  /// NewCallback() allocates a new, single-use
  /// <code>CompletionCallback</code>.
  /// The <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param method The method taking the callback. Method should be of type:
  /// <code>
  /// void (T::*)(int32_t result, const A& a, const B& b, const C& c)
  /// </code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] c Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B, typename C>
  CompletionCallback NewCallback(Method method, const A& a, const B& b,
                                 const C& c) {
    return NewCallbackHelper(new Dispatcher3<Method, A, B, C>(method, a, b, c));
  }

  /// NewOptionalCallback() allocates a new, single-use
  /// <code>CompletionCallback</code> that might not run if the method
  /// taking it can complete synchronously. Thus, if after passing the
  /// CompletionCallback to a Pepper method, the method does not return
  /// PP_OK_COMPLETIONPENDING, then you should manually call the
  /// CompletionCallback's Run method, or memory will be leaked.
  ///
  /// @param[in] method The method taking the callback. Method should be of
  /// type:
  /// <code>
  /// void (T::*)(int32_t result, const A& a, const B& b, const C& c)
  /// </code>
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] c Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Method, typename A, typename B, typename C>
  CompletionCallback NewOptionalCallback(Method method, const A& a,
                                         const B& b, const C& c) {
    CompletionCallback cc = NewCallback(method, a, b, c);
    cc.set_flags(cc.flags() | PP_COMPLETIONCALLBACK_FLAG_OPTIONAL);
    return cc;
  }

  /// NewCallbackWithOutput() allocates a new, single-use
  /// <code>CompletionCallback</code> where the browser will pass an additional
  /// parameter containing the result of the request. The
  /// <code>CompletionCallback</code> must be run in order for the memory
  /// allocated by the methods to be freed.
  ///
  /// @param method The method to be run.
  ///
  /// @param[in] a Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] b Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @param[in] c Passed to <code>method</code> when the completion callback
  /// runs.
  ///
  /// @return A <code>CompletionCallback</code>.
  template <typename Output, typename A, typename B, typename C>
  CompletionCallbackWithOutput<
      typename internal::TypeUnwrapper<Output>::StorageType>
  NewCallbackWithOutput(void (T::*method)(int32_t, Output, A, B, C),
                        const A& a,
                        const B& b,
                        const C& c) {
    return NewCallbackWithOutputHelper(new DispatcherWithOutput3<
        typename internal::TypeUnwrapper<Output>::StorageType,
        void (T::*)(int32_t, Output, A, B, C),
        typename internal::TypeUnwrapper<A>::StorageType,
        typename internal::TypeUnwrapper<B>::StorageType,
        typename internal::TypeUnwrapper<C>::StorageType>(method, a, b, c));
  }

 private:
  class BackPointer {
   public:
    typedef CompletionCallbackFactory<T, ThreadTraits> FactoryType;

    explicit BackPointer(FactoryType* factory)
        : factory_(factory) {
    }

    void AddRef() {
      ref_.AddRef();
    }

    void Release() {
      if (ref_.Release() == 0)
        delete this;
    }

    void DropFactory() {
      factory_ = NULL;
    }

    T* GetObject() {
      return factory_ ? factory_->GetObject() : NULL;
    }

   private:
    typename ThreadTraits::RefCount ref_;
    FactoryType* factory_;
  };

  template <typename Dispatcher>
  class CallbackData {
   public:
    // Takes ownership of the given dispatcher pointer.
    CallbackData(BackPointer* back_pointer, Dispatcher* dispatcher)
        : back_pointer_(back_pointer),
          dispatcher_(dispatcher) {
      back_pointer_->AddRef();
    }

    ~CallbackData() {
      back_pointer_->Release();
      delete dispatcher_;
    }

    Dispatcher* dispatcher() { return dispatcher_; }

    static void Thunk(void* user_data, int32_t result) {
      Self* self = static_cast<Self*>(user_data);
      T* object = self->back_pointer_->GetObject();

      // Please note that |object| may be NULL at this point. But we still need
      // to call into Dispatcher::operator() in that case, so that it can do
      // necessary cleanup.
      (*self->dispatcher_)(object, result);

      delete self;
    }

   private:
    typedef CallbackData<Dispatcher> Self;
    BackPointer* back_pointer_;  // We own a ref to this refcounted object.
    Dispatcher* dispatcher_;  // We own this pointer.

    // Disallow copying & assignment.
    CallbackData(const CallbackData<Dispatcher>&);
    CallbackData<Dispatcher>& operator=(const CallbackData<Dispatcher>&);
  };

  template <typename Method>
  class Dispatcher0 {
   public:
    Dispatcher0() : method_(NULL) {}
    explicit Dispatcher0(Method method) : method_(method) {
    }
    void operator()(T* object, int32_t result) {
      if (object)
        (object->*method_)(result);
    }
   private:
    Method method_;
  };

  template <typename Output, typename Method>
  class DispatcherWithOutput0 {
   public:
    typedef Output OutputType;
    typedef internal::CallbackOutputTraits<Output> Traits;

    DispatcherWithOutput0()
        : method_(NULL),
          output_() {
      Traits::Initialize(&output_);
    }
    DispatcherWithOutput0(Method method)
        : method_(method),
          output_() {
      Traits::Initialize(&output_);
    }
    void operator()(T* object, int32_t result) {
      // We must call Traits::StorageToPluginArg() even if we don't need to call
      // the callback anymore, otherwise we may leak resource or var references.
      if (object)
        (object->*method_)(result, Traits::StorageToPluginArg(output_));
      else
        Traits::StorageToPluginArg(output_);
    }
    typename Traits::StorageType* output() {
      return &output_;
    }
   private:
    Method method_;

    typename Traits::StorageType output_;
  };

  template <typename Method, typename A>
  class Dispatcher1 {
   public:
    Dispatcher1()
        : method_(NULL),
          a_() {
    }
    Dispatcher1(Method method, const A& a)
        : method_(method),
          a_(a) {
    }
    void operator()(T* object, int32_t result) {
      if (object)
        (object->*method_)(result, a_);
    }
   private:
    Method method_;
    A a_;
  };

  template <typename Output, typename Method, typename A>
  class DispatcherWithOutput1 {
   public:
    typedef Output OutputType;
    typedef internal::CallbackOutputTraits<Output> Traits;

    DispatcherWithOutput1()
        : method_(NULL),
          a_(),
          output_() {
      Traits::Initialize(&output_);
    }
    DispatcherWithOutput1(Method method, const A& a)
        : method_(method),
          a_(a),
          output_() {
      Traits::Initialize(&output_);
    }
    void operator()(T* object, int32_t result) {
      // We must call Traits::StorageToPluginArg() even if we don't need to call
      // the callback anymore, otherwise we may leak resource or var references.
      if (object)
        (object->*method_)(result, Traits::StorageToPluginArg(output_), a_);
      else
        Traits::StorageToPluginArg(output_);
    }
    typename Traits::StorageType* output() {
      return &output_;
    }
   private:
    Method method_;
    A a_;

    typename Traits::StorageType output_;
  };

  template <typename Method, typename A, typename B>
  class Dispatcher2 {
   public:
    Dispatcher2()
        : method_(NULL),
          a_(),
          b_() {
    }
    Dispatcher2(Method method, const A& a, const B& b)
        : method_(method),
          a_(a),
          b_(b) {
    }
    void operator()(T* object, int32_t result) {
      if (object)
        (object->*method_)(result, a_, b_);
    }
   private:
    Method method_;
    A a_;
    B b_;
  };

  template <typename Output, typename Method, typename A, typename B>
  class DispatcherWithOutput2 {
   public:
    typedef Output OutputType;
    typedef internal::CallbackOutputTraits<Output> Traits;

    DispatcherWithOutput2()
        : method_(NULL),
          a_(),
          b_(),
          output_() {
      Traits::Initialize(&output_);
    }
    DispatcherWithOutput2(Method method, const A& a, const B& b)
        : method_(method),
          a_(a),
          b_(b),
          output_() {
      Traits::Initialize(&output_);
    }
    void operator()(T* object, int32_t result) {
      // We must call Traits::StorageToPluginArg() even if we don't need to call
      // the callback anymore, otherwise we may leak resource or var references.
      if (object)
        (object->*method_)(result, Traits::StorageToPluginArg(output_), a_, b_);
      else
        Traits::StorageToPluginArg(output_);
    }
    typename Traits::StorageType* output() {
      return &output_;
    }
   private:
    Method method_;
    A a_;
    B b_;

    typename Traits::StorageType output_;
  };

  template <typename Method, typename A, typename B, typename C>
  class Dispatcher3 {
   public:
    Dispatcher3()
        : method_(NULL),
          a_(),
          b_(),
          c_() {
    }
    Dispatcher3(Method method, const A& a, const B& b, const C& c)
        : method_(method),
          a_(a),
          b_(b),
          c_(c) {
    }
    void operator()(T* object, int32_t result) {
      if (object)
        (object->*method_)(result, a_, b_, c_);
    }
   private:
    Method method_;
    A a_;
    B b_;
    C c_;
  };

  template <typename Output, typename Method, typename A, typename B,
            typename C>
  class DispatcherWithOutput3 {
   public:
    typedef Output OutputType;
    typedef internal::CallbackOutputTraits<Output> Traits;

    DispatcherWithOutput3()
        : method_(NULL),
          a_(),
          b_(),
          c_(),
          output_() {
      Traits::Initialize(&output_);
    }
    DispatcherWithOutput3(Method method, const A& a, const B& b, const C& c)
        : method_(method),
          a_(a),
          b_(b),
          c_(c),
          output_() {
      Traits::Initialize(&output_);
    }
    void operator()(T* object, int32_t result) {
      // We must call Traits::StorageToPluginArg() even if we don't need to call
      // the callback anymore, otherwise we may leak resource or var references.
      if (object) {
        (object->*method_)(result, Traits::StorageToPluginArg(output_),
                           a_, b_, c_);
      } else {
        Traits::StorageToPluginArg(output_);
      }
    }
    typename Traits::StorageType* output() {
      return &output_;
    }
   private:
    Method method_;
    A a_;
    B b_;
    C c_;

    typename Traits::StorageType output_;
  };

  // Creates the back pointer object and takes a reference to it. This assumes
  // either that the lock is held or that it is not needed.
  void InitBackPointer() {
    back_pointer_ = new BackPointer(this);
    back_pointer_->AddRef();
  }

  // Releases our reference to the back pointer object and clears the pointer.
  // This assumes either that the lock is held or that it is not needed.
  void ResetBackPointer() {
    back_pointer_->DropFactory();
    back_pointer_->Release();
    back_pointer_ = NULL;
  }

  // Takes ownership of the dispatcher pointer, which should be heap allocated.
  template <typename Dispatcher>
  CompletionCallback NewCallbackHelper(Dispatcher* dispatcher) {
    typename ThreadTraits::AutoLock lock(lock_);

    PP_DCHECK(object_);  // Expects a non-null object!
    return CompletionCallback(
        &CallbackData<Dispatcher>::Thunk,
        new CallbackData<Dispatcher>(back_pointer_, dispatcher));
  }

  // Takes ownership of the dispatcher pointer, which should be heap allocated.
  template <typename Dispatcher> CompletionCallbackWithOutput<
      typename internal::TypeUnwrapper<
          typename Dispatcher::OutputType>::StorageType>
  NewCallbackWithOutputHelper(Dispatcher* dispatcher) {
    typename ThreadTraits::AutoLock lock(lock_);

    PP_DCHECK(object_);  // Expects a non-null object!
    CallbackData<Dispatcher>* data =
        new CallbackData<Dispatcher>(back_pointer_, dispatcher);

    return CompletionCallbackWithOutput<typename Dispatcher::OutputType>(
        &CallbackData<Dispatcher>::Thunk,
        data,
        data->dispatcher()->output());
  }

  // Disallowed:
  CompletionCallbackFactory(const CompletionCallbackFactory&);
  CompletionCallbackFactory& operator=(const CompletionCallbackFactory&);

  // Never changed once initialized so does not need protection by the lock.
  T* object_;

  // Protects the back pointer.
  typename ThreadTraits::Lock lock_;

  // Protected by the lock. This will get reset when you do CancelAll, for
  // example.
  BackPointer* back_pointer_;
};

}  // namespace pp

#endif  // PPAPI_UTILITY_COMPLETION_CALLBACK_FACTORY_H_
