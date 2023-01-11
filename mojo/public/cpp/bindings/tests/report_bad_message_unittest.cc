// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "mojo/public/cpp/bindings/message.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/tests/bindings_test_base.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/interfaces/bindings/tests/test_bad_messages.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace mojo {
namespace test {
namespace {

class TestBadMessagesImpl : public TestBadMessages {
 public:
  TestBadMessagesImpl() = default;

  TestBadMessagesImpl(const TestBadMessagesImpl&) = delete;
  TestBadMessagesImpl& operator=(const TestBadMessagesImpl&) = delete;

  ~TestBadMessagesImpl() override = default;

  void Bind(PendingReceiver<TestBadMessages> receiver) {
    receiver_.Bind(std::move(receiver));
  }

  ReportBadMessageCallback& bad_message_callback() {
    return bad_message_callback_;
  }

 private:
  // TestBadMessages:
  void RejectEventually(RejectEventuallyCallback callback) override {
    bad_message_callback_ = GetBadMessageCallback();
    std::move(callback).Run();
  }

  void RequestResponse(RequestResponseCallback callback) override {
    std::move(callback).Run();
  }

  void RejectSync(RejectSyncCallback callback) override {
    std::move(callback).Run();
    ReportBadMessage("go away");
  }

  void RequestResponseSync(RequestResponseSyncCallback callback) override {
    std::move(callback).Run();
  }

  ReportBadMessageCallback bad_message_callback_;
  mojo::Receiver<TestBadMessages> receiver_{this};
};

class ReportBadMessageTest : public BindingsTestBase {
 public:
  ReportBadMessageTest() = default;

  void SetUp() override {
    mojo::SetDefaultProcessErrorHandler(base::BindRepeating(
        &ReportBadMessageTest::OnProcessError, base::Unretained(this)));

    impl_.Bind(remote_.BindNewPipeAndPassReceiver());
  }

  void TearDown() override {
    mojo::SetDefaultProcessErrorHandler(base::NullCallback());
  }

  TestBadMessages* remote() { return remote_.get(); }

  TestBadMessagesImpl* impl() { return &impl_; }

  void SetErrorHandler(base::OnceClosure handler) {
    error_handler_ = std::move(handler);
  }

 private:
  void OnProcessError(const std::string& error) {
    if (error_handler_)
      std::move(error_handler_).Run();
  }

  Remote<TestBadMessages> remote_;
  TestBadMessagesImpl impl_;
  base::OnceClosure error_handler_;
};

TEST_P(ReportBadMessageTest, Request) {
  // Verify that basic immediate error reporting works.
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));
  EXPECT_TRUE(remote()->RejectSync());
  EXPECT_TRUE(error);
}

TEST_P(ReportBadMessageTest, RequestAsync) {
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  // This should capture a bad message reporting callback in the impl.
  base::RunLoop loop;
  remote()->RejectEventually(loop.QuitClosure());
  loop.Run();

  EXPECT_FALSE(error);

  // Now we can run the callback and it should trigger a bad message report.
  DCHECK(!impl()->bad_message_callback().is_null());
  std::move(impl()->bad_message_callback()).Run("bad!");
  EXPECT_TRUE(error);
}

TEST_P(ReportBadMessageTest, Response) {
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  base::RunLoop loop;
  remote()->RequestResponse(base::BindLambdaForTesting([&] {
    // Report a bad message inside the response callback. This should
    // trigger the error handler.
    ReportBadMessage("no way!");
    loop.Quit();
  }));
  loop.Run();

  EXPECT_TRUE(error);
}

TEST_P(ReportBadMessageTest, ResponseAsync) {
  bool error = false;
  SetErrorHandler(base::BindLambdaForTesting([&] { error = true; }));

  ReportBadMessageCallback bad_message_callback;
  base::RunLoop loop;
  remote()->RequestResponse(base::BindLambdaForTesting([&] {
    // Capture the bad message callback inside the response callback.
    bad_message_callback = GetBadMessageCallback();
    loop.Quit();
  }));
  loop.Run();

  EXPECT_FALSE(error);

  // Invoking this callback should report a bad message and trigger the error
  // handler immediately.
  std::move(bad_message_callback)
      .Run("this message is bad and should feel bad");
  EXPECT_TRUE(error);
}

INSTANTIATE_MOJO_BINDINGS_TEST_SUITE_P(ReportBadMessageTest);

}  // namespace
}  // namespace test
}  // namespace mojo
