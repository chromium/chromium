// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_TEST_CALLBACK_RECEIVER_H_
#define DEVICE_FIDO_TEST_CALLBACK_RECEIVER_H_

#include <tuple>
#include <type_traits>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/run_loop.h"

namespace device {
namespace test {

// Serves as a testing callback target on which callbacks with an arbitrary
// signature `void(CallbackArgs...)` can be invoked on.
//
// Example usage:
//   base::test::TaskEnvironment task_environment;
//   TestCallbackReceiver<int> callback_receiver;
//
//   // Manufacture the base::OnceCallback whose invovcation will be received
//   // by this instance.
//   auto callback = callback_receiver.callback();
//
//   // Pass |callback| into testing code that will invoke it.
//   DoStuffAndInvokeCallbackWithResult(std::move(callback));
//
//   // Spin the message loop until the callback is invoked, and read result.
//   callback_receiver.WaitForCallback();
//   DoStuffWithResult(std::get<0>(*callback_receiver.result());
//
template <class... CallbackArgs>
class TestCallbackReceiver {
 public:
  using TupleOfNonReferenceArgs = std::tuple<std::decay_t<CallbackArgs>...>;

  TestCallbackReceiver() = default;
  ~TestCallbackReceiver() = default;

  // Whether the |callback| was already called.
  bool was_called() const { return was_called_; }

  // The result, which is non-null exactly if the callback was already invoked
  // and the result has not yet been taken with TakeResult().
  const base::Optional<TupleOfNonReferenceArgs>& result() const {
    return result_;
  }

  // Constructs a base::OnceCallback that can be passed into code under test and
  // be waited, but must not be invoked after |this| instance goes out of scope.
  //
  // This method can only be called once during the lifetime of an instance.
  // Construct multiple TestCallbackReceiver instances for multiple callbacks.
  base::OnceCallback<void(CallbackArgs...)> callback() {
    return base::BindOnce(&TestCallbackReceiver::ReceiverMethod,
                          base::Unretained(this));
  }

  // Takes a tuple containing the arguments the callback was called with.
  TupleOfNonReferenceArgs TakeResult() {
    auto value = std::move(result_).value();
    result_.reset();
    return value;
  }

  // Returns immediately if the |callback()| was already called, otherwise pumps
  // the current MessageLoop until it is called.
  void WaitForCallback() {
    if (was_called_)
      return;
    wait_for_callback_loop_.Run();
  }

 private:
  void ReceiverMethod(CallbackArgs... args) {
    result_.emplace(std::forward<CallbackArgs>(args)...);
    was_called_ = true;
    wait_for_callback_loop_.Quit();
  }

  bool was_called_ = false;
  base::RunLoop wait_for_callback_loop_;
  base::Optional<TupleOfNonReferenceArgs> result_;

  DISALLOW_COPY_AND_ASSIGN(TestCallbackReceiver);
};

template <class Value>
class ValueCallbackReceiver : public TestCallbackReceiver<Value> {
 public:
  const Value& value() const {
    return std::get<0>(*TestCallbackReceiver<Value>::result());
  }
};

template <class Status, class Value>
class StatusAndValueCallbackReceiver
    : public TestCallbackReceiver<Status, Value> {
 public:
  const Status& status() const {
    return std::get<0>(*TestCallbackReceiver<Status, Value>::result());
  }

  const Value& value() const {
    return std::get<1>(*TestCallbackReceiver<Status, Value>::result());
  }
};

template <class Status, class... Values>
class StatusAndValuesCallbackReceiver
    : public TestCallbackReceiver<Status, Values...> {
 public:
  const Status& status() const {
    return std::get<0>(*TestCallbackReceiver<Status, Values...>::result());
  }

  template <size_t I>
  const std::tuple_element_t<I, std::tuple<Values...>>& value() const {
    return std::get<I + 1>(*TestCallbackReceiver<Status, Values...>::result());
  }
};

}  // namespace test
}  // namespace device

#endif  // DEVICE_FIDO_TEST_CALLBACK_RECEIVER_H_
