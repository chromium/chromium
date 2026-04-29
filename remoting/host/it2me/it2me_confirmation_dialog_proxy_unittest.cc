// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/it2me/it2me_confirmation_dialog_proxy.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "base/synchronization/waitable_event.h"
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
      : task_runner_(task_runner) {}
  ~StubIt2MeConfirmationDialog() override {
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
  }

  void ReportResult(Result result) {
    ASSERT_TRUE(task_runner_->BelongsToCurrentThread());
    std::move(callback_).Run(result);
  }

  bool inputs_disabled() const { return inputs_disabled_; }

  MOCK_METHOD(void, OnShow, ());

  // It2MeConfirmationDialog implementation.
  void Show(const std::string& remote_user_email,
            ResultCallback callback) override {
    EXPECT_TRUE(callback_.is_null());
    EXPECT_TRUE(task_runner_->BelongsToCurrentThread());
    EXPECT_EQ(remote_user_email.compare(kTestEmailAddress), 0);
    // At the moment Show() is called, inputs should have been disabled by
    // the proxy's draining logic.
    EXPECT_TRUE(inputs_disabled_);
    callback_ = std::move(callback);
    OnShow();
  }

  void SetDisableInputs(bool disable) override {
    inputs_disabled_ = disable;
    OnSetDisableInputs(disable);
  }

  MOCK_METHOD1(OnSetDisableInputs, void(bool));

 private:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  ResultCallback callback_;
  bool inputs_disabled_ = false;
};

// Encapsulates a target for It2MeConfirmationDialog::ResultCallback.
class ResultCallbackTarget {
 public:
  explicit ResultCallbackTarget(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner)
      : task_runner_(task_runner) {}

  MOCK_METHOD(void, OnDialogResult, (It2MeConfirmationDialog::Result));

  It2MeConfirmationDialog::ResultCallback MakeCallback() {
    return base::BindOnce(&ResultCallbackTarget::HandleDialogResult,
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

  void Run() { run_loop_.Run(); }

  void Quit() { run_loop_.Quit(); }

  It2MeConfirmationDialogProxy* dialog_proxy() { return dialog_proxy_.get(); }

  StubIt2MeConfirmationDialog* dialog() { return dialog_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::Thread dialog_thread_;

  // |dialog_| is owned by |dialog_proxy_| but we keep an alias for testing.
  // This dangling raw_ptr occurred in:
  // remoting_unittests: It2MeConfirmationDialogProxyTest.Show
  // https://ci.chromium.org/ui/p/chromium/builders/try/linux-rel/1425645/test-results?q=ExactID%3Aninja%3A%2F%2Fremoting%3Aremoting_unittests%2FIt2MeConfirmationDialogProxyTest.Show+VHash%3A5b63361209a49b2c
  raw_ptr<StubIt2MeConfirmationDialog, FlakyDanglingUntriaged> dialog_ =
      nullptr;
  std::unique_ptr<It2MeConfirmationDialogProxy> dialog_proxy_;
};

It2MeConfirmationDialogProxyTest::It2MeConfirmationDialogProxyTest()
    : dialog_thread_("test dialog thread") {
  dialog_thread_.Start();

  auto dialog =
      std::make_unique<StubIt2MeConfirmationDialog>(dialog_task_runner());
  dialog_ = dialog.get();
  dialog_proxy_ = std::make_unique<It2MeConfirmationDialogProxy>(
      dialog_task_runner(), std::move(dialog));
}

It2MeConfirmationDialogProxyTest::~It2MeConfirmationDialogProxyTest() = default;

TEST_F(It2MeConfirmationDialogProxyTest, Show) {
  ResultCallbackTarget callback_target(main_task_runner());

  StubIt2MeConfirmationDialog* confirm_dialog = dialog();

  EXPECT_CALL(*dialog(), OnSetDisableInputs(::testing::_))
      .Times(::testing::AtLeast(2));
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

TEST_F(It2MeConfirmationDialogProxyTest, DrainingAndDisabling) {
  ResultCallbackTarget callback_target(main_task_runner());

  StubIt2MeConfirmationDialog* confirm_dialog = dialog();

  // Sequence of events we expect on the dialog thread:
  // 1. OnSetDisableInputs(true)
  // 2. OnShow()
  // 3. OnSetDisableInputs(false)

  ::testing::InSequence s;
  EXPECT_CALL(*dialog(), OnSetDisableInputs(true));
  EXPECT_CALL(*dialog(), OnShow());
  EXPECT_CALL(*dialog(), OnSetDisableInputs(false))
      .WillOnce(InvokeWithoutArgs([confirm_dialog]() {
        confirm_dialog->ReportResult(It2MeConfirmationDialog::Result::OK);
      }));

  EXPECT_CALL(callback_target,
              OnDialogResult(It2MeConfirmationDialog::Result::OK))
      .WillOnce(
          InvokeWithoutArgs(this, &It2MeConfirmationDialogProxyTest::Quit));

  dialog_proxy()->Show(kTestEmailAddress, callback_target.MakeCallback());

  Run();
  }

  TEST_F(It2MeConfirmationDialogProxyTest, EventDraining) {
  ResultCallbackTarget callback_target(main_task_runner());
  StubIt2MeConfirmationDialog* confirm_dialog = dialog();

  // Setup expectations FIRST to avoid race conditions with background thread.
  // We expect these calls on the dialog thread.
  EXPECT_CALL(*dialog(), OnSetDisableInputs(true));
  EXPECT_CALL(*dialog(), OnShow());
  EXPECT_CALL(*dialog(), OnSetDisableInputs(false));

  // Setup the default action for OnShow to report the result.
  ON_CALL(*dialog(), OnShow())
      .WillByDefault(InvokeWithoutArgs([confirm_dialog]() {
        confirm_dialog->ReportResult(It2MeConfirmationDialog::Result::OK);
      }));

  // We expect this call on the main thread.
  EXPECT_CALL(callback_target,
              OnDialogResult(It2MeConfirmationDialog::Result::OK))
      .WillOnce(
          InvokeWithoutArgs(this, &It2MeConfirmationDialogProxyTest::Quit));

  base::WaitableEvent blocker_event;

  // We want to guarantee this exact order on the dialog thread:
  // 1. blocker_task (blocks until signaled)
  // 2. Core::Show (posted by proxy->Show)
  // 3. simulated_input_task (posted by test)
  // 4. Core::ShowAfterDrain (posted by Core::Show)

  // 1. Post blocker.
  dialog_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](base::WaitableEvent* event) {
                       event->Wait();
                     },
                     base::Unretained(&blocker_event)));

  // 2. Call Show(). This posts Core::Show to the dialog thread.
  dialog_proxy()->Show(kTestEmailAddress, callback_target.MakeCallback());

  // 3. Post a "simulated input" task to the dialog thread.
  dialog_task_runner()->PostTask(
      FROM_HERE, base::BindOnce(
                     [](StubIt2MeConfirmationDialog* dialog) {
                       // At this point, Core::Show should have executed,
                       // calling SetDisableInputs(true) and posting
                       // ShowAfterDrain.
                       EXPECT_TRUE(dialog->inputs_disabled());
                     },
                     base::Unretained(confirm_dialog)));

  // Now release the blocker. Tasks will run in the guaranteed order.
  blocker_event.Signal();

  Run();

  // After the run loop quits, we need to ensure the dialog thread has
  // finished its remaining work (like re-enabling inputs) before the
  // test ends and mocks are destroyed.
  base::RunLoop fencing_run_loop;
  dialog_task_runner()->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                         fencing_run_loop.QuitClosure());
  fencing_run_loop.Run();
}

}  // namespace remoting
