// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/message_pipe.h"
#include "mojo/public/cpp/test_support/test_support.h"
#include "mojo/public/interfaces/bindings/tests/sample_interfaces.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

///////////////////////////////////////////////////////////////////////////////
//
// The tests in this file are designed to test the interaction between a
// Callback and its associated Receiver. If a Callback is deleted before
// being used we DCHECK fail--unless the associated Receiver has already
// been closed or deleted. This contract must be explained to the Mojo
// application developer. For example it is the developer's responsibility to
// ensure that the Receiver is destroyed before an unused Callback is destroyed.
//
///////////////////////////////////////////////////////////////////////////////

namespace mojo {
namespace test {
namespace {

// An implementation of sample::Provider used on the server side.
// It only implements one of the methods: EchoInt().
// All it does is save the values and Callbacks it sees.
class InterfaceImpl : public sample::Provider {
 public:
  InterfaceImpl() : last_server_value_seen_(0) {}

  ~InterfaceImpl() override {}

  // Run's the callback previously saved from the last invocation
  // of |EchoInt()|.
  bool RunCallback() {
    if (callback_saved_) {
      std::move(callback_saved_).Run(last_server_value_seen_);
      return true;
    }
    return false;
  }

  // Delete's the previously saved callback.
  void DeleteCallback() { callback_saved_.Reset(); }

  // sample::Provider implementation

  // Saves its two input values in member variables and does nothing else.
  void EchoInt(int32_t x, EchoIntCallback callback) override {
    last_server_value_seen_ = x;
    callback_saved_ = std::move(callback);
    if (closure_)
      std::move(closure_).Run();
  }

  void EchoString(const std::string& a, EchoStringCallback callback) override {
    CHECK(false) << "Not implemented.";
  }

  void EchoStrings(const std::string& a,
                   const std::string& b,
                   EchoStringsCallback callback) override {
    CHECK(false) << "Not implemented.";
  }

  void EchoMessagePipeHandle(ScopedMessagePipeHandle a,
                             EchoMessagePipeHandleCallback callback) override {
    CHECK(false) << "Not implemented.";
  }

  void EchoEnum(sample::Enum a, EchoEnumCallback callback) override {
    CHECK(false) << "Not implemented.";
  }

  void ResetLastServerValueSeen() { last_server_value_seen_ = 0; }

  int32_t last_server_value_seen() const { return last_server_value_seen_; }

  void set_closure(base::OnceClosure closure) { closure_ = std::move(closure); }

 private:
  int32_t last_server_value_seen_;
  EchoIntCallback callback_saved_;
  base::OnceClosure closure_;
};

class ReceiverCallbackTest : public testing::Test {
 public:
  ReceiverCallbackTest() {}
  ~ReceiverCallbackTest() override {}

 protected:
  int32_t last_client_callback_value_seen_;
  Remote<sample::Provider> remote_;

  void PumpMessages() { base::RunLoop().RunUntilIdle(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

// Tests that the Remote and the Receiver can communicate with each other
// normally.
TEST_F(ReceiverCallbackTest, Basic) {
  // Create the ServerImpl and the Receiver.
  InterfaceImpl server_impl;
  Receiver<sample::Provider> receiver(&server_impl,
                                      remote_.BindNewPipeAndPassReceiver());

  // Initialize the test values.
  server_impl.ResetLastServerValueSeen();
  last_client_callback_value_seen_ = 0;

  // Invoke the Echo method.
  base::RunLoop run_loop, run_loop2;
  server_impl.set_closure(run_loop.QuitClosure());
  remote_->EchoInt(7, base::BindLambdaForTesting([&](int32_t value) {
                     last_client_callback_value_seen_ = value;
                     run_loop2.Quit();
                   }));
  run_loop.Run();

  // Check that server saw the correct value, but the client has not yet.
  EXPECT_EQ(7, server_impl.last_server_value_seen());
  EXPECT_EQ(0, last_client_callback_value_seen_);

  // Now run the Callback.
  server_impl.RunCallback();
  run_loop2.Run();

  // Check that the client has now seen the correct value.
  EXPECT_EQ(7, last_client_callback_value_seen_);

  // Initialize the test values again.
  server_impl.ResetLastServerValueSeen();
  last_client_callback_value_seen_ = 0;

  // Invoke the Echo method again.
  base::RunLoop run_loop3, run_loop4;
  server_impl.set_closure(run_loop3.QuitClosure());
  remote_->EchoInt(13, base::BindLambdaForTesting([&](int32_t value) {
                     last_client_callback_value_seen_ = value;
                     run_loop4.Quit();
                   }));
  run_loop3.Run();

  // Check that server saw the correct value, but the client has not yet.
  EXPECT_EQ(13, server_impl.last_server_value_seen());
  EXPECT_EQ(0, last_client_callback_value_seen_);

  // Now run the Callback again.
  server_impl.RunCallback();
  run_loop4.Run();

  // Check that the client has now seen the correct value again.
  EXPECT_EQ(13, last_client_callback_value_seen_);
}

// Tests that running the Callback after the Receiver has been deleted
// results in a clean failure.
TEST_F(ReceiverCallbackTest, DeleteReceiverThenRunCallback) {
  // Create the ServerImpl.
  InterfaceImpl server_impl;
  base::RunLoop run_loop;
  {
    // Create the receiver in an inner scope so it can be deleted first.
    Receiver<sample::Provider> receiver(&server_impl,
                                        remote_.BindNewPipeAndPassReceiver());
    remote_.set_disconnect_handler(run_loop.QuitClosure());

    // Initialize the test values.
    server_impl.ResetLastServerValueSeen();
    last_client_callback_value_seen_ = 0;

    // Invoke the Echo method.
    base::RunLoop run_loop2;
    server_impl.set_closure(run_loop2.QuitClosure());
    remote_->EchoInt(7, base::BindLambdaForTesting([&](int32_t value) {
                       last_client_callback_value_seen_ = value;
                     }));
    run_loop2.Run();
  }
  // The receiver has now been destroyed and its pipe endpoint is closed.

  // Check that server saw the correct value, but the client has not yet.
  EXPECT_EQ(7, server_impl.last_server_value_seen());
  EXPECT_EQ(0, last_client_callback_value_seen_);

  // Now try to run the Callback. This should do nothing since the pipe
  // is closed.
  EXPECT_TRUE(server_impl.RunCallback());
  PumpMessages();

  // Check that the client has still not seen the correct value.
  EXPECT_EQ(0, last_client_callback_value_seen_);

  // Attempt to invoke the method again and confirm that an error was
  // encountered.
  remote_->EchoInt(13, base::BindLambdaForTesting([&](int32_t value) {
                     last_client_callback_value_seen_ = value;
                     run_loop.Quit();
                   }));
  run_loop.Run();
  EXPECT_FALSE(remote_.is_connected());
}

// Tests that deleting a Callback without running it after the corresponding
// Receiver has already been deleted does not result in a crash.
TEST_F(ReceiverCallbackTest, DeleteReceiverThenDeleteCallback) {
  // Create the ServerImpl.
  InterfaceImpl server_impl;
  {
    // Create the receiver in an inner scope so it can be deleted first.
    Receiver<sample::Provider> receiver(&server_impl,
                                        remote_.BindNewPipeAndPassReceiver());

    // Initialize the test values.
    server_impl.ResetLastServerValueSeen();
    last_client_callback_value_seen_ = 0;

    // Invoke the Echo method.
    base::RunLoop run_loop;
    server_impl.set_closure(run_loop.QuitClosure());
    remote_->EchoInt(7, base::BindLambdaForTesting([&](int32_t value) {
                       last_client_callback_value_seen_ = value;
                     }));
    run_loop.Run();
  }
  // The receiver has now been destroyed and its pipe endpoint is closed.

  // Check that server saw the correct value, but the client has not yet.
  EXPECT_EQ(7, server_impl.last_server_value_seen());
  EXPECT_EQ(0, last_client_callback_value_seen_);

  // Delete the callback without running it. This should not
  // cause a problem because the infrastructure can detect that the
  // receiver has already been destroyed.
  server_impl.DeleteCallback();
}

// Tests that closing a Receiver allows us to delete a callback
// without running it without encountering a crash.
TEST_F(ReceiverCallbackTest, ResetReceiverBeforeDeletingCallback) {
  // Create the ServerImpl and the Receiver.
  InterfaceImpl server_impl;
  Receiver<sample::Provider> receiver(&server_impl,
                                      remote_.BindNewPipeAndPassReceiver());

  // Initialize the test values.
  server_impl.ResetLastServerValueSeen();
  last_client_callback_value_seen_ = 0;

  // Invoke the Echo method.
  base::RunLoop run_loop;
  server_impl.set_closure(run_loop.QuitClosure());
  remote_->EchoInt(7, base::BindLambdaForTesting([&](int32_t value) {
                     last_client_callback_value_seen_ = value;
                   }));
  run_loop.Run();

  // Check that server saw the correct value, but the client has not yet.
  EXPECT_EQ(7, server_impl.last_server_value_seen());
  EXPECT_EQ(0, last_client_callback_value_seen_);

  // Now disconnect the Receiver.
  receiver.reset();

  // Delete the callback without running it. This should not
  // cause a crash because the infrastructure can detect that the
  // receiver has already been closed.
  server_impl.DeleteCallback();

  // Check that the client has still not seen the correct value.
  EXPECT_EQ(0, last_client_callback_value_seen_);
}

// Tests that deleting a Callback without using it before the
// Receiver has been destroyed or closed results in a DCHECK.
TEST_F(ReceiverCallbackTest, DeleteCallbackBeforeReceiverDeathTest) {
  // Create the ServerImpl and the Receiver.
  InterfaceImpl server_impl;
  Receiver<sample::Provider> receiver(&server_impl,
                                      remote_.BindNewPipeAndPassReceiver());

  // Initialize the test values.
  server_impl.ResetLastServerValueSeen();
  last_client_callback_value_seen_ = 0;

  // Invoke the Echo method.
  base::RunLoop run_loop;
  server_impl.set_closure(run_loop.QuitClosure());
  remote_->EchoInt(7, base::BindLambdaForTesting([&](int32_t value) {
                     last_client_callback_value_seen_ = value;
                   }));
  run_loop.Run();

  // Check that server saw the correct value, but the client has not yet.
  EXPECT_EQ(7, server_impl.last_server_value_seen());
  EXPECT_EQ(0, last_client_callback_value_seen_);

  EXPECT_DCHECK_DEATH(server_impl.DeleteCallback());
}

}  // namespace
}  // namespace test
}  // namespace mojo
