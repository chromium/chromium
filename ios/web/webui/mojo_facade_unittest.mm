// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "base/test/test_future.h"
#import "base/values.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/mojo_test.test-mojom.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {


// Deserializes the given `json` to an object.
id GetObject(const std::string& json) {
  NSData* json_as_data =
      [base::SysUTF8ToNSString(json) dataUsingEncoding:NSUTF8StringEncoding];
  return [NSJSONSerialization JSONObjectWithData:json_as_data
                                         options:0
                                           error:nil];
}

class FakeWebStateWithInterfaceBinder : public FakeWebState {
 public:
  InterfaceBinder* GetInterfaceBinderForMainFrame() override {
    return &interface_binder_;
  }

 private:
  InterfaceBinder interface_binder_{this};
};

}  // namespace

// A test fixture to test MojoFacade class.
class MojoFacadeTest : public WebTest {
 protected:
  MojoFacadeTest() {
    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));
    facade_ = std::make_unique<MojoFacade>(&web_state_);

    auto main_frame = FakeWebFrame::Create("frameID", /*is_main_frame=*/true);

    main_frame_ = main_frame.get();
    frames_manager_->AddWebFrame(std::move(main_frame));
    main_frame_->ClearJavaScriptCallHistory();
  }

  FakeWebFrame* main_frame() { return main_frame_; }
  MojoFacade* facade() { return facade_.get(); }
  FakeWebStateWithInterfaceBinder& web_state() { return web_state_; }

  std::string HandleMessage(const base::DictValue& message) {
    base::test::TestFuture<int, std::string> future;
    facade()->HandleMojoMessage(0, &message, future.GetCallback());
    return future.Get<1>();
  }

  void CreateMessagePipe(uint32_t* handle0, uint32_t* handle1) {
    int handle0_id = next_handle_id_++;
    int handle1_id = next_handle_id_++;
    base::DictValue create;
    create.Set("name", "Mojo.createMessagePipe");
    base::DictValue args;
    args.Set("handle0Id", handle0_id);
    args.Set("handle1Id", handle1_id);
    create.Set("args", std::move(args));
    std::string response_as_string = HandleMessage(create);

    // Verify handles.
    ASSERT_FALSE(response_as_string.empty());
    NSDictionary* response_as_dict = GetObject(response_as_string);
    ASSERT_TRUE([response_as_dict isKindOfClass:[NSDictionary class]]);
    ASSERT_EQ(MOJO_RESULT_OK, [response_as_dict[@"result"] unsignedIntValue]);
    *handle0 = [response_as_dict[@"handle0"] unsignedIntValue];
    *handle1 = [response_as_dict[@"handle1"] unsignedIntValue];
  }

  void CloseHandle(uint32_t handle) {
    base::DictValue close;
    close.Set("name", "MojoHandle.close");
    base::DictValue args;
    args.Set("handle", static_cast<int>(handle));
    close.Set("args", std::move(args));
    std::string result = HandleMessage(close);
    EXPECT_TRUE(result.empty());
  }

  int WatchHandle(uint32_t handle, int callback_id) {
    base::DictValue watch;
    watch.Set("name", "MojoHandle.watch");
    base::DictValue args;
    args.Set("handle", static_cast<int>(handle));
    args.Set("signals", static_cast<int>(MOJO_HANDLE_SIGNAL_READABLE));
    args.Set("callbackId", callback_id);
    watch.Set("args", std::move(args));
    const std::string watch_id_as_string = HandleMessage(watch);
    EXPECT_FALSE(watch_id_as_string.empty());
    int watch_id = 0;
    EXPECT_TRUE(base::StringToInt(watch_id_as_string, &watch_id));
    return watch_id;
  }

  void CancelWatch(uint32_t handle, int watch_id) {
    base::DictValue cancel_watch;
    cancel_watch.Set("name", "MojoWatcher.cancel");
    base::DictValue args;
    args.Set("watchId", watch_id);
    cancel_watch.Set("args", std::move(args));
    EXPECT_TRUE(HandleMessage(cancel_watch).empty());
  }

  void WriteMessage(uint32_t handle, std::string_view buffer) {
    base::DictValue write;
    write.Set("name", "MojoHandle.writeMessage");
    base::DictValue args;
    args.Set("handle", static_cast<int>(handle));
    args.Set("handles", base::ListValue());
    args.Set("buffer", buffer);
    write.Set("args", std::move(args));
    const std::string result_as_string = HandleMessage(write);
    EXPECT_FALSE(result_as_string.empty());
    unsigned result = 0u;
    EXPECT_TRUE(base::StringToUint(result_as_string, &result));
    EXPECT_EQ(MOJO_RESULT_OK, result);
  }

  std::string WaitForLastJavaScriptCall() {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(
        kWaitForJSCompletionTimeout, /*run_message_loop=*/true, ^bool {
          return !main_frame()->GetLastJavaScriptCall().empty();
        }));

    const auto last_js_call = main_frame()->GetLastJavaScriptCall();
    main_frame()->ClearJavaScriptCallHistory();
    return base::UTF16ToUTF8(last_js_call);
  }

  std::string GetExpectedWatchCallbackScript(uint32_t handle, int callback_id) {
    return base::StringPrintf(
        "Mojo.internal.fetchNextMessageFromNative(%d, "
        "{\"buffer\":[65,66,67,68],\"handles\":[],\"result\":0}); "
        "Mojo.internal.watchCallbacksHolder.callCallback(%d, %d);",
        handle, callback_id, MOJO_RESULT_OK);
  }

 private:
  int next_handle_id_ = 1;
  FakeWebStateWithInterfaceBinder web_state_;
  raw_ptr<web::FakeWebFramesManager> frames_manager_;
  raw_ptr<FakeWebFrame> main_frame_;
  std::unique_ptr<MojoFacade> facade_;
};

// Tests binding an interface.
TEST_F(MojoFacadeTest, BindInterface) {
  uint32_t handle0 = 0;
  uint32_t handle1 = 0;
  CreateMessagePipe(&handle0, &handle1);

  // Pass handle0 as interface request.
  base::DictValue connect;
  connect.Set("name", "Mojo.bindInterface");
  base::DictValue args;
  args.Set("interfaceName", ".TestUIHandlerMojo");
  args.Set("requestHandle", static_cast<int>(handle0));
  connect.Set("args", std::move(args));

  std::string handle_as_string = HandleMessage(connect);
  EXPECT_TRUE(handle_as_string.empty());

  CloseHandle(handle1);
}

// Tests watching the pipe.
TEST_F(MojoFacadeTest, Watch) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  // Start watching one end of the pipe.
  const int kCallbackId = 99;
  WatchHandle(handle0, kCallbackId);

  // Write to the other end of the pipe.
  WriteMessage(handle1, "QUJDRA==");  // "ABCD" in base-64

  EXPECT_EQ(GetExpectedWatchCallbackScript(handle0, kCallbackId),
            WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

// Tests that a watcher is automatically re-armed after its callback runs.
TEST_F(MojoFacadeTest, WatcherRearming) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  // Start watching one end of the pipe.
  const int kCallbackId = 99;
  WatchHandle(handle0, kCallbackId);

  // Write to the other end of the pipe.
  WriteMessage(handle1, "QUJDRA==");  // "ABCD" in base-64

  EXPECT_EQ(GetExpectedWatchCallbackScript(handle0, kCallbackId),
            WaitForLastJavaScriptCall());

  // Write to the other end of the pipe.
  WriteMessage(handle1, "QUJDRA==");  // "ABCD" in base-64

  // Check the watcher was rearmed and works.
  EXPECT_EQ(GetExpectedWatchCallbackScript(handle0, kCallbackId),
            WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

// Tests canceling a handle watcher stops future notifications.
TEST_F(MojoFacadeTest, CancelWatch) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  // Make 2 watchers on one end of the pipe.
  const int kCallbackId1 = 99;
  const int kCallbackId2 = 101;
  WatchHandle(handle0, kCallbackId1);
  const int watch_id2 = WatchHandle(handle0, kCallbackId2);
  const auto expected_script2 = base::StringPrintf(
      "Mojo.internal.fetchNextMessageFromNative(%d, "
      "{\"result\":%d}); "
      "Mojo.internal.watchCallbacksHolder.callCallback(%d, %d);",
      handle0, MOJO_RESULT_SHOULD_WAIT, kCallbackId2, MOJO_RESULT_OK);

  // Write to the other end of the pipe.
  WriteMessage(handle1, "QUJDRA==");  // "ABCD" in base-64

  // `expected_script1` is also called, but GetLastJavaScriptCall() will store
  // only the last one.
  EXPECT_EQ(expected_script2, WaitForLastJavaScriptCall());

  // Cancel the second watcher and write again.
  CancelWatch(handle0, watch_id2);
  WriteMessage(handle1, "QUJDRA==");  // "ABCD" in base-64

  // Only the first watcher should be notified.
  EXPECT_EQ(GetExpectedWatchCallbackScript(handle0, kCallbackId1),
            WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

// Tests reading and writing messages on a pipe via watchers.
TEST_F(MojoFacadeTest, ReadWrite) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  const int kCallbackId = 123;
  WatchHandle(handle0, kCallbackId);

  // Write to the other end of the pipe.
  WriteMessage(handle1, "QUJDRA==");  // "ABCD" in base-64

  EXPECT_EQ(GetExpectedWatchCallbackScript(handle0, kCallbackId),
            WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

// Tests that HasRegisteredInterfaces() correctly reflects registered
// interfaces.
TEST_F(MojoFacadeTest, HasRegisteredInterfaces) {
  WebState::InterfaceBinder* binder =
      web_state().GetInterfaceBinderForMainFrame();
  EXPECT_FALSE(binder->HasRegisteredInterfaces());

  binder->AddInterface("FakeInterface",
                       base::BindRepeating([](mojo::GenericPendingReceiver*) {
                         // Do nothing.
                       }));
  EXPECT_TRUE(binder->HasRegisteredInterfaces());

  binder->RemoveInterface("FakeInterface");
  EXPECT_FALSE(binder->HasRegisteredInterfaces());
}

// Tests that a page load failure does not start message polling.
TEST_F(MojoFacadeTest, PageLoadedFailureDoesNotStartPolling) {
  main_frame()->ClearJavaScriptCallHistory();
  web_state().OnPageLoaded(PageLoadCompletionStatus::FAILURE);
  EXPECT_TRUE(main_frame()->GetLastJavaScriptCall().empty());
}

// Tests that receiving an invalid message during polling is handled safely.
TEST_F(MojoFacadeTest, AwaitNextMessageWithInvalidMessage) {
  base::Value invalid_msg(base::Value::Type::DICT);
  main_frame()->AddResultForExecutedJs(
      &invalid_msg, u"return await Mojo.internal.fetchNextMessageFromJS();");

  main_frame()->ClearJavaScriptCallHistory();
  facade()->AwaitNextMessage();

  EXPECT_EQ("return await Mojo.internal.fetchNextMessageFromJS();",
            WaitForLastJavaScriptCall());

  main_frame()->AddResultForExecutedJs(
      nullptr, u"return await Mojo.internal.fetchNextMessageFromJS();");
}

// Tests that calling AwaitNextMessage while a poll is active does not trigger
// duplicate JavaScript calls.
TEST_F(MojoFacadeTest, AwaitNextMessagePreventsConcurrentPolls) {
  main_frame()->ClearJavaScriptCallHistory();

  facade()->AwaitNextMessage();
  EXPECT_EQ(1u, main_frame()->GetJavaScriptCallHistory().size());

  facade()->AwaitNextMessage();
  EXPECT_EQ(1u, main_frame()->GetJavaScriptCallHistory().size());
}

}  // namespace web
