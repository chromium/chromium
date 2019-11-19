// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::InvokeWithoutArgs;

namespace remoting {

namespace {
const char kTestEmailAddress[] = "faux_remote_user@chromium_test.com";
}  // namespace

class StubIt2MeConfirmationDialog : public It2MeConfirmationDialog {
 public:
  explicit StubIt2MeConfirmationDialog(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
  }
  ~StubIt2MeConfirmationDialog() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void ReportResult(Result result) {
    ASSERT_TRUE(task_runner_->BelongsToCurrentThread());
    callback_.Run(result);
  }

  MOCK_METHOD0(OnShow, void());

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            const ResultCallback& callback) override {
    EXPECT_TRUE(callback_.is_null());
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_EQ(remote_user_email.compare(kTestEmailAddress), 0);
    callback_ = callback;
    OnShow();
  }

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ResultCallback callback_;
};

// Encapsulates a target for It2MeConfirmationDialog::ResultCallback.
class ResultCallbackTarget {
 public:
  explicit ResultCallbackTarget(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {
  }

  MOCK_METHOD1(OnDialogResult, void(It2MeConfirmationDialog::Result));

  It2MeConfirmationDialog::ResultCallback MakeCallback() {
    return base::Bind(&ResultCallbackTarget::HandleDialogResult,
                      base::Unretained(this));
  }

 private:
  void HandleDialogResult(It2MeConfirmationDialog::Result result) {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    OnDialogResult(result);
  }

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
};

class It2MeConfirmationDialogProxyTest : public testing::Test {
 public:
  It2MeConfirmationDialogProxyTest();
  ~It2MeConfirmationDialogProxyTest() override;

  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

  scoped_refptr<base::SingleThreadTaskRunner> dialog_task_runner() {
    return dialog_thread_.task_runner();
  }

  void Run() {
    run_loop_.Run();
  }

  void Quit() {
    run_loop_.Quit();
  }

  It2MeConfirmationDialogProxy* dialog_proxy() {
    return dialog_proxy_.get();
  }

  StubIt2MeConfirmationDialog* dialog() {
    return dialog_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::Thread dialog_thread_;

  // |dialog_| is owned by |dialog_proxy_| but we keep an alias for testing.
  StubIt2MeConfirmationDialog* dialog_ = nullptr;
  std::unique_ptr<It2MeConfirmationDialogProxy> dialog_proxy_;
};

It2MeConfirmationDialogProxyTest::It2MeConfirmationDialogProxyTest()
    : dialog_thread_("test dialog thread") {
  dialog_thread_.Start();

  auto dialog =
      std::make_unique<StubIt2MeConfirmationDialog>(dialog_task_runner());
  dialog_ = dialog.get();
  dialog_proxy_.reset(new It2MeConfirmationDialogProxy(dialog_task_runner(),
                                                       std::move(dialog)));
}

It2MeConfirmationDialogProxyTest::~It2MeConfirmationDialogProxyTest() = default;

TEST_F(It2MeConfirmationDialogProxyTest, Show) {
  ResultCallbackTarget callback_target(main_task_runner());

  StubIt2MeConfirmationDialog* confirm_dialog = dialog();
  EXPECT_CALL(*dialog(), OnShow())
      .WillOnce(InvokeWithoutArgs([confirm_dialog]() {
        confirm_dialog->ReportResult(It2MeConfirmationDialog::Result::CANCEL);
      }));

  EXPECT_CALL(callback_target,
              OnDialogResult(It2MeConfirmationDialog::Result::CANCEL))
      .WillOnce(
          InvokeWithoutArgs(this, &It2MeConfirmationDialogProxyTest::Quit));

  dialog_proxy()->Show(kTestEmailAddress, callback_target.MakeCallback());

  Run();
}

}  // namespace remoting
