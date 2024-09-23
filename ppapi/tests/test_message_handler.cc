// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_message_handler.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <sstream>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/c/ppb_messaging.h"
#include "ppapi/c/ppp_message_handler.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/message_handler.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/network_list.h"
#include "ppapi/cpp/network_monitor.h"
#include "ppapi/cpp/var.h"
#include "ppapi/cpp/var_array.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/var_dictionary.h"
#include "ppapi/tests/pp_thread.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

// Windows defines 'PostMessage', so we have to undef it.
#ifdef PostMessage
#undef PostMessage
#endif

REGISTER_TEST_CASE(MessageHandler);

namespace {

// Created and destroyed on the main thread. All public methods should be called
// on the main thread. Most data members are only accessed on the main thread.
// (Though it handles messages on the background thread).
class MyMessageHandler : public pp::MessageHandler {
 public:
  explicit MyMessageHandler(TestingInstance* instance,
                            const pp::MessageLoop& loop)
      : testing_instance_(instance),
        message_handler_loop_(loop),
        is_registered_(false),
        test_finished_event_(instance->pp_instance()),
        destroy_event_(instance->pp_instance()),
        async_message_received_(instance->pp_instance()) {
    AssertOnMainThread();
  }
  void Register() {
    AssertOnMainThread();
    assert(!is_registered_);
    int32_t result =
        testing_instance_->RegisterMessageHandler(this, message_handler_loop_);
    if (result == PP_OK) {
      is_registered_ = true;
    } else {
      std::ostringstream stream;
      stream << "Failed to register message handler; got error " << result;
      AddError(stream.str());
      test_finished_event_.Signal();
    }
    // Note, at this point, we can't safely read or write errors_ until we wait
    // on destroy_event_.
  }
  void Unregister() {
    AssertOnMainThread();
    assert(is_registered_);
    testing_instance_->UnregisterMessageHandler();
    is_registered_ = false;
  }
  void WaitForTestFinishedMessage() {
    test_finished_event_.Wait();
    test_finished_event_.Reset();
  }
  // Wait for Destroy() to be called on the MessageHandler thread. When it's
  // done, return any errors that occurred during the time the MessageHandler
  // was getting messages.
  std::string WaitForDestroy() {
    AssertOnMainThread();
    // If we haven't called Unregister, we'll be waiting forever.
    assert(!is_registered_);
    destroy_event_.Wait();
    destroy_event_.Reset();
    // Now that we know Destroy() has been called, we know errors_ isn't being
    // written on the MessageHandler thread anymore. So we can safely read it
    // here on the main thread (since destroy_event_ gave us a memory barrier).
    std::string temp_errors;
    errors_.swap(temp_errors);
    return temp_errors;
  }
  pp::Var WaitForAsyncMessage() {
    async_message_received_.Wait();
    pp::Var var_to_return = last_async_message_received_;
    last_async_message_received_ = pp::Var();
    async_message_received_.Reset();
    return var_to_return;
  }
 private:
  static void AssertOnMainThread() {
    assert(pp::MessageLoop::GetForMainThread() ==
           pp::MessageLoop::GetCurrent());
  }
  void AddError(const std::string& error) {
    if (!error.empty()) {
      if (!errors_.empty())
        errors_ += "<p>";
      errors_ += error;
    }
  }
  virtual void HandleMessage(pp::InstanceHandle instance, const pp::Var& var) {
    if (pp::MessageLoop::GetCurrent() != message_handler_loop_)
      AddError("HandleMessage was called on the wrong thread!");
    if (instance.pp_instance() != testing_instance_->pp_instance())
      AddError("HandleMessage was passed the wrong instance!");
    if (var.is_string() && var.AsString() == "FINISHED_TEST") {
      test_finished_event_.Signal();
    } else {
      // Any client causing a message to arrive must wait for the message
      // before continuing. See WaitForAsyncMessage().
      assert(last_async_message_received_.is_undefined());
      last_async_message_received_ = var;
      async_message_received_.Signal();
    }
  }

  virtual pp::Var HandleBlockingMessage(pp::InstanceHandle instance,
                                        const pp::Var& var) {
    if (pp::MessageLoop::GetCurrent() != message_handler_loop_)
      AddError("HandleBlockingMessage was called on the wrong thread!");
    if (instance.pp_instance() != testing_instance_->pp_instance())
      AddError("HandleBlockingMessage was passed the wrong instance!");

    // Attempt a blocking operation; make sure it's disallowed.
    pp::NetworkMonitor monitor(instance);
    PP_Resource out_param = 0;
    pp::CompletionCallbackWithOutput<pp::NetworkList> blocking_callback(
        &out_param);
    int32_t error = monitor.UpdateNetworkList(blocking_callback);
    if (error != PP_ERROR_WOULD_BLOCK_THREAD) {
      AddError("HandleBlockingMessage was allowed to do a blocking call!");
      pp::Module::Get()->core()->ReleaseResource(out_param);
    }

    return var;
  }

  virtual void WasUnregistered(pp::InstanceHandle instance) {
    if (pp::MessageLoop::GetCurrent() != message_handler_loop_)
      AddError("Destroy was called on the wrong thread!");
    if (instance.pp_instance() != testing_instance_->pp_instance())
      AddError("Destroy was passed the wrong instance!");
    destroy_event_.Signal();
  }

  // These data members are initialized on the main thread, but don't change for
  // the life of the object, so are safe to access on the background thread,
  // because there will be a memory barrier before the the MessageHandler calls
  // are invoked.
  TestingInstance* const testing_instance_;
  const pp::MessageLoop message_handler_loop_;
  const pp::MessageLoop main_loop_;

  // is_registered_ is only read/written on the main thread.
  bool is_registered_;

  // errors_ is written on the MessageHandler thread. When Destroy() is
  // called, we stop writing to errors_ and signal destroy_event_. This causes
  // a memory barrier, so it's safe to read errors_ after that.
  std::string errors_;
  NestedEvent test_finished_event_;
  NestedEvent destroy_event_;

  pp::Var last_async_message_received_;
  NestedEvent async_message_received_;

  // Undefined & private to disallow copy and assign.
  MyMessageHandler(const MyMessageHandler&);
  MyMessageHandler& operator=(const MyMessageHandler&);
};

void FakeHandleMessage(PP_Instance instance,
                       void* user_data,
                       const PP_Var* message_data) {}
void FakeHandleBlockingMessage(PP_Instance instance,
                               void* user_data,
                               const PP_Var* message_data,
                               PP_Var* result) {}
void FakeDestroy(PP_Instance instance, void* user_data) {}

}  // namespace

TestMessageHandler::TestMessageHandler(TestingInstance* instance)
    : TestCase(instance),
      message_received_(instance->pp_instance()),
      ppb_messaging_if_(NULL),
      handler_thread_(instance) {
}

TestMessageHandler::~TestMessageHandler() {
  handler_thread_.Join();
}

bool TestMessageHandler::Init() {
  ppb_messaging_if_ = static_cast<const PPB_Messaging_1_2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_MESSAGING_INTERFACE_1_2));
  return ppb_messaging_if_ &&
         CheckTestingInterface() &&
         handler_thread_.Start();
}

void TestMessageHandler::RunTests(const std::string& filter) {
  RUN_TEST(RegisterErrorConditions, filter);
  RUN_TEST(PostMessageAndAwaitResponse, filter);
  RUN_TEST(ArrayBuffer, filter);
  RUN_TEST(Exceptions, filter);
}

void TestMessageHandler::HandleMessage(const pp::Var& message_data) {
  if (instance()->current_test_name() == "Exceptions") {
    // For TestPostMessageAndAwaitResponse(), all messages should go to the
    // background thread message handler.
    assert(false);
  } else {
    // Any subtest causing a message to arrive here must wait for it before
    // continuing. See WaitForMessage().
    assert(last_message_.is_undefined());
    last_message_ = message_data;
    message_received_.Signal();
  }
}

std::string TestMessageHandler::TestRegisterErrorConditions() {
  {
    // Test registering with the main thread as the message loop.
    PPP_MessageHandler_0_2 fake_ppp_message_handler = {
      &FakeHandleMessage, &FakeHandleBlockingMessage, &FakeDestroy
    };
    pp::MessageLoop main_loop = pp::MessageLoop::GetForMainThread();
    int32_t result = ppb_messaging_if_->RegisterMessageHandler(
        instance()->pp_instance(),
        reinterpret_cast<void*>(0xdeadbeef),
        &fake_ppp_message_handler,
        main_loop.pp_resource());
    ASSERT_EQ(PP_ERROR_WRONG_THREAD, result);
  }
  {
    // Test registering with incomplete PPP_Messaging interface.
    PPP_MessageHandler_0_2 bad_ppp_ifs[] = {
        { NULL, &FakeHandleBlockingMessage, &FakeDestroy },
        { &FakeHandleMessage, NULL, &FakeDestroy },
        { &FakeHandleMessage, &FakeHandleBlockingMessage, NULL }};
    for (size_t i = 0; i < sizeof(bad_ppp_ifs)/sizeof(bad_ppp_ifs[0]); ++i) {
      int32_t result = ppb_messaging_if_->RegisterMessageHandler(
          instance()->pp_instance(),
          reinterpret_cast<void*>(0xdeadbeef),
          &bad_ppp_ifs[i],
          handler_thread_.message_loop().pp_resource());
      ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
    }
  }
  PASS();
}

std::string TestMessageHandler::TestPostMessageAndAwaitResponse() {
  MyMessageHandler handler(instance(),
                           handler_thread_.message_loop());
  handler.Register();
  std::string js_code("var plugin = document.getElementById('plugin');\n");
  js_code += "var result = undefined;\n";
  const char* const values_to_test[] = {
      "5",
      "undefined",
      "1.5",
      "'hello'",
      "{'key': 'value', 'array_key': [1, 2, 3, 4, 5]}",
      NULL
  };
  for (size_t i = 0; values_to_test[i]; ++i) {
    js_code += "result = plugin.postMessageAndAwaitResponse(";
    js_code +=     values_to_test[i];
    js_code += ");\n";
    js_code += "if (!deepCompare(result, ";
    js_code +=     values_to_test[i];
    js_code += "))\n";
    js_code += "  InternalError(\" Failed postMessageAndAwaitResponse for: ";
    js_code +=      values_to_test[i];
    js_code +=    " result: \" + result);\n";
  }
  instance_->EvalScript(js_code);
  instance_->EvalScript("plugin.postMessage('FINISHED_TEST');\n");
  handler.WaitForTestFinishedMessage();
  handler.Unregister();
  ASSERT_SUBTEST_SUCCESS(handler.WaitForDestroy());

  PASS();
}

std::string TestMessageHandler::TestArrayBuffer() {
  // Set the array buffer shared memory threshold so that some of the
  // ArrayBuffer values will be sent as shared memory.
  ScopedArrayBufferSizeSetter setter(testing_interface_,
                                     instance_->pp_instance(),
                                     200);
  const char* const sizes[] = { "0", "128", "1024", "4096", NULL };
  MyMessageHandler handler(instance(),
                           handler_thread_.message_loop());
  handler.Register();
  std::string js_code("var plugin = document.getElementById('plugin');\n");
  js_code += "var result = undefined;\n";
  js_code += "var param = undefined;\n";
  for (size_t i = 0; sizes[i]; ++i) {
    js_code += "param = new ArrayBuffer(";
    js_code += sizes[i];
    js_code += ");";
    // TODO(dmichael): It would be better to set specific values in param.
    js_code += "result = plugin.postMessageAndAwaitResponse(param);";
    js_code += "if (!deepCompare(result, param))\n";
    js_code += "  InternalError(\" Failed postMessageAndAwaitResponse for ";
    js_code += "ArrayBuffer of size: ";
    js_code +=     sizes[i];
    js_code += " result: \" + result);\n";
  }
  instance_->EvalScript(js_code);
  instance_->EvalScript("plugin.postMessage('FINISHED_TEST');\n");
  handler.WaitForTestFinishedMessage();
  handler.Unregister();
  ASSERT_SUBTEST_SUCCESS(handler.WaitForDestroy());

  PASS();
}

std::string TestMessageHandler::TestExceptions() {
  MyMessageHandler handler(instance(),
                           handler_thread_.message_loop());
  {
    // First, try sending a blocking message when there is no handler
    // registered. It should throw an exception.
    std::string js_code(
        "var plugin = document.getElementById('plugin');\n"
        "var caught_exception = false;\n"
        "try {\n"
        "  plugin.postMessageAndAwaitResponse('Hello!');\n"
        "} catch (err) {\n"
        "  caught_exception = true;\n"
        "}\n"
        "plugin.postMessage(caught_exception ? 'SUCCESS' : 'FAIL');\n");
    instance_->EvalScript(js_code);
    // Note that we want to wait for the Instance to get the SUCCESS/FAIL
    // message here. |message_handler| is not yet registered, so the message
    // goes to the instance instead.
    pp::Var msg = WaitForMessage();
    ASSERT_TRUE(msg.is_string());
    ASSERT_EQ("SUCCESS", msg.AsString());
  }
  handler.Register();
  {
    // Now that a handler is registered, try requesting and sending a
    // FileSystem. It should throw an exception. The file system is opened
    // asynchronously. What *should* happen is that it opens successfully, then
    // we try to send it via postMessageAndAwaitResponse, which fails with an
    // exception. The test could fail either because the filesystem doesn't
    // open or because postMessageAndAwaitResponse doesn't throw an exception.
    std::string js_code(
        "var plugin = document.getElementById('plugin');\n"
        "function gotFileSystem(fs) {\n"
        "  var caught_exception = false;\n"
        "  try {\n"
        "    plugin.postMessageAndAwaitResponse(fs);\n"
        "  } catch (err) {\n"
        "    caught_exception = true;\n"
        "  }\n"
        "  plugin.postMessage(caught_exception ? 'SUCCESS' : 'FAIL');\n"
        "}\n"
        "function fileSystemError() {\n"
        "  plugin.postMessage('Failed to open filesystem');\n"
        "}\n"
        "window.webkitRequestFileSystem(\n"
        "    window.Temporary, 1024, gotFileSystem, fileSystemError)\n");
    instance_->EvalScript(js_code);
    pp::Var msg = handler.WaitForAsyncMessage();
    ASSERT_EQ(PP_VARTYPE_STRING, msg.pp_var().type);
    ASSERT_EQ("SUCCESS", msg.AsString());
  }
  handler.Unregister();
  ASSERT_SUBTEST_SUCCESS(handler.WaitForDestroy());

  PASS();
}

pp::Var TestMessageHandler::WaitForMessage() {
  message_received_.Wait();
  pp::Var var_to_return = last_message_;
  last_message_ = pp::Var();
  message_received_.Reset();
  return var_to_return;
}

