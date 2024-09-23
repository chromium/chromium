// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#import <memory>

#import "base/functional/bind.h"
#import "base/memory/raw_ptr.h"
#import "base/run_loop.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "base/test/ios/wait_util.h"
#import "ios/web/public/test/fakes/fake_web_frame.h"
#import "ios/web/public/test/fakes/fake_web_frames_manager.h"
#import "ios/web/public/test/fakes/fake_web_state.h"
#import "ios/web/public/test/web_test.h"
#import "ios/web/test/mojo_test.mojom.h"
#import "ios/web/web_state/web_state_impl.h"
#import "testing/gtest_mac.h"

using base::test::ios::kWaitForJSCompletionTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace web {

namespace {

// Serializes the given `object` to JSON string.
std::string GetJson(id object) {
  NSData* json_as_data =
      [NSJSONSerialization dataWithJSONObject:object options:0 error:nil];
  NSString* json_as_string =
      [[NSString alloc] initWithData:json_as_data
                            encoding:NSUTF8StringEncoding];
  return base::SysNSStringToUTF8(json_as_string);
}

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
    facade_ = std::make_unique<MojoFacade>(&web_state_);

    auto web_frames_manager = std::make_unique<web::FakeWebFramesManager>();
    frames_manager_ = web_frames_manager.get();
    web_state_.SetWebFramesManager(std::move(web_frames_manager));

    auto main_frame =
        FakeWebFrame::Create("frameID", /*is_main_frame=*/true, GURL());

    main_frame_ = main_frame.get();
    frames_manager_->AddWebFrame(std::move(main_frame));
  }

  FakeWebFrame* main_frame() { return main_frame_; }
  MojoFacade* facade() { return facade_.get(); }

  void CreateMessagePipe(uint32_t* handle0, uint32_t* handle1) {
    NSDictionary* create = @{
      @"name" : @"Mojo.createMessagePipe",
      @"args" : @{},
    };
    std::string response_as_string =
        facade()->HandleMojoMessage(GetJson(create));

    // Verify handles.
    ASSERT_FALSE(response_as_string.empty());
    NSDictionary* response_as_dict = GetObject(response_as_string);
    ASSERT_TRUE([response_as_dict isKindOfClass:[NSDictionary class]]);
    ASSERT_EQ(MOJO_RESULT_OK, [response_as_dict[@"result"] unsignedIntValue]);
    *handle0 = [response_as_dict[@"handle0"] unsignedIntValue];
    *handle1 = [response_as_dict[@"handle1"] unsignedIntValue];
  }

  void CloseHandle(uint32_t handle) {
    NSDictionary* close = @{
      @"name" : @"MojoHandle.close",
      @"args" : @{
        @"handle" : @(handle),
      },
    };
    std::string result = facade()->HandleMojoMessage(GetJson(close));
    EXPECT_TRUE(result.empty());
  }

  NSDictionary* ReadMessage(uint32_t handle) {
    // Read the message from the pipe.
    NSDictionary* read = @{
      @"name" : @"MojoHandle.readMessage",
      @"args" : @{
        @"handle" : @(handle),
      },
    };
    NSDictionary* message =
        GetObject(facade()->HandleMojoMessage(GetJson(read)));
    EXPECT_TRUE([message isKindOfClass:[NSDictionary class]]);
    return message;
  }

  int WatchHandle(uint32_t handle, int callback_id) {
    NSDictionary* watch = @{
      @"name" : @"MojoHandle.watch",
      @"args" : @{
        @"handle" : @(handle),
        @"signals" : @(MOJO_HANDLE_SIGNAL_READABLE),
        @"callbackId" : @(callback_id),
      },
    };
    const std::string watch_id_as_string =
        facade()->HandleMojoMessage(GetJson(watch));
    EXPECT_FALSE(watch_id_as_string.empty());
    int watch_id = 0;
    EXPECT_TRUE(base::StringToInt(watch_id_as_string, &watch_id));
    return watch_id;
  }

  void CancelWatch(uint32_t handle, int watch_id) {
    NSDictionary* cancel_watch = @{
      @"name" : @"MojoWatcher.cancel",
      @"args" : @{
        @"watchId" : @(watch_id),
      },
    };
    EXPECT_TRUE(facade_->HandleMojoMessage(GetJson(cancel_watch)).empty());
  }

  void WriteMessage(uint32_t handle, NSString* buffer) {
    NSDictionary* write = @{
      @"name" : @"MojoHandle.writeMessage",
      @"args" : @{@"handle" : @(handle), @"handles" : @[], @"buffer" : buffer},
    };
    const std::string result_as_string =
        facade()->HandleMojoMessage(GetJson(write));
    EXPECT_FALSE(result_as_string.empty());
    unsigned result = 0u;
    EXPECT_TRUE(base::StringToUint(result_as_string, &result));
    EXPECT_EQ(MOJO_RESULT_OK, result);
  }

  std::string WaitForLastJavaScriptCall() {
    EXPECT_TRUE(WaitUntilConditionOrTimeout(kWaitForJSCompletionTimeout, ^bool {
      // Flush any pending tasks. Don't RunUntilIdle() because
      // RunUntilIdle() is incompatible with mojo::SimpleWatcher's
      // automatic arming behavior, which Mojo JS still depends upon.
      base::RunLoop loop;
      base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, loop.QuitClosure());
      loop.Run();
      return !main_frame()->GetLastJavaScriptCall().empty();
    }));

    const auto last_js_call = main_frame()->GetLastJavaScriptCall();
    main_frame()->ClearJavaScriptCallHistory();
    return base::UTF16ToUTF8(last_js_call);
  }

 private:
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
  NSDictionary* connect = @{
    @"name" : @"Mojo.bindInterface",
    @"args" : @{
      @"interfaceName" : @".TestUIHandlerMojo",
      @"requestHandle" : @(handle0),
    },
  };

  std::string handle_as_string = facade()->HandleMojoMessage(GetJson(connect));
  EXPECT_TRUE(handle_as_string.empty());

  CloseHandle(handle1);
}

// Tests creating a message pipe.
TEST_F(MojoFacadeTest, CreateMessagePipe) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  CloseHandle(handle0);
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
  WriteMessage(handle1, @"QUJDRA==");  // "ABCD" in base-64

  const auto expected_script = base::StringPrintf(
      "Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)", kCallbackId,
      MOJO_RESULT_OK);
  EXPECT_EQ(expected_script, WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

TEST_F(MojoFacadeTest, WatcherRearming) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  // Start watching one end of the pipe.
  const int kCallbackId = 99;
  WatchHandle(handle0, kCallbackId);
  const auto expected_script = base::StringPrintf(
      "Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)", kCallbackId,
      MOJO_RESULT_OK);

  // Write to the other end of the pipe.
  WriteMessage(handle1, @"QUJDRA==");  // "ABCD" in base-64

  EXPECT_EQ(expected_script, WaitForLastJavaScriptCall());

  // Read the pipe until MOJO_RESULT_SHOULD_WAIT is returned
  // (the usual watcher behavior).
  EXPECT_EQ([ReadMessage(handle0)[@"result"] unsignedIntValue], MOJO_RESULT_OK);
  EXPECT_EQ([ReadMessage(handle0)[@"result"] unsignedIntValue],
            MOJO_RESULT_SHOULD_WAIT);

  // Write to the other end of the pipe.
  WriteMessage(handle1, @"QUJDRA==");  // "ABCD" in base-64

  // Check the watcher was reamed and works.
  EXPECT_EQ(expected_script, WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

TEST_F(MojoFacadeTest, CancelWatch) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  // Make 2 watchers on one end of the pipe.
  const int kCallbackId1 = 99;
  const int kCallbackId2 = 101;
  WatchHandle(handle0, kCallbackId1);
  const int watch_id2 = WatchHandle(handle0, kCallbackId2);
  const auto expected_script1 = base::StringPrintf(
      "Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)", kCallbackId1,
      MOJO_RESULT_OK);
  const auto expected_script2 = base::StringPrintf(
      "Mojo.internal.watchCallbacksHolder.callCallback(%d, %d)", kCallbackId2,
      MOJO_RESULT_OK);

  // Write to the other end of the pipe.
  WriteMessage(handle1, @"QUJDRA==");  // "ABCD" in base-64

  // `expected_script1` is also called, but GetLastJavaScriptCall() will store
  // only the last one.
  EXPECT_EQ(expected_script2, WaitForLastJavaScriptCall());

  // Read the pipe until MOJO_RESULT_SHOULD_WAIT is returned
  // (the usual watcher behavior).
  EXPECT_EQ([ReadMessage(handle0)[@"result"] unsignedIntValue], MOJO_RESULT_OK);
  EXPECT_EQ([ReadMessage(handle0)[@"result"] unsignedIntValue],
            MOJO_RESULT_SHOULD_WAIT);

  // Cancel the second watcher and write again.
  CancelWatch(handle0, watch_id2);
  WriteMessage(handle1, @"QUJDRA==");  // "ABCD" in base-64

  // Only the second watcher should be notified.
  EXPECT_EQ(expected_script1, WaitForLastJavaScriptCall());

  CloseHandle(handle0);
  CloseHandle(handle1);
}

// Tests reading the message from the pipe.
TEST_F(MojoFacadeTest, ReadWrite) {
  uint32_t handle0, handle1;
  CreateMessagePipe(&handle0, &handle1);

  // Write to the other end of the pipe.
  WriteMessage(handle1, @"QUJDRA==");  // "ABCD" in base-64

  // Read the message from the pipe.
  NSDictionary* message = ReadMessage(handle0);
  NSArray* expected_message = @[ @65, @66, @67, @68 ];  // ASCII values for A, B, C, D
  EXPECT_NSEQ(expected_message, message[@"buffer"]);
  EXPECT_FALSE([message[@"handles"] count]);
  EXPECT_EQ(MOJO_RESULT_OK, [message[@"result"] unsignedIntValue]);

  CloseHandle(handle0);
  CloseHandle(handle1);
}

}  // namespace web
