// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_post_message.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <algorithm>
#include <map>
#include <sstream>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_file_io.h"
#include "ppapi/cpp/file_io.h"
#include "ppapi/cpp/file_ref.h"
#include "ppapi/cpp/file_system.h"
#include "ppapi/cpp/instance.h"
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

REGISTER_TEST_CASE(PostMessage);

namespace {

const char kTestFilename[] = "testfile.txt";
const char kTestString[] = "Hello world!";
const bool kTestBool = true;
const int32_t kTestInt = 42;
const double kTestDouble = 42.0;

// On Windows XP bots, the NonMainThread test can run very slowly. So we dial
// back the number of threads & messages when running on Windows.
#ifdef PPAPI_OS_WIN
const int32_t kThreadsToRun = 2;
const int32_t kMessagesToSendPerThread = 5;
#else
const int32_t kThreadsToRun = 4;
const int32_t kMessagesToSendPerThread = 10;
#endif

// The struct that invoke_post_message_thread_func expects for its argument.
// It includes the instance on which to invoke PostMessage, and the value to
// pass to PostMessage.
struct InvokePostMessageThreadArg {
  InvokePostMessageThreadArg(pp::Instance* i, const pp::Var& v)
      : instance(i), value_to_send(v) {}
  pp::Instance* instance;
  pp::Var value_to_send;
};

void InvokePostMessageThreadFunc(void* user_data) {
  InvokePostMessageThreadArg* arg =
      static_cast<InvokePostMessageThreadArg*>(user_data);
  for (int32_t i = 0; i < kMessagesToSendPerThread; ++i)
    arg->instance->PostMessage(arg->value_to_send);
  delete arg;
}

// TODO(raymes): Consider putting something like this into pp::Var.
bool VarsEqual(const pp::Var& expected,
               const pp::Var& actual,
               std::map<int64_t, int64_t>* visited_ids) {
  if (expected.pp_var().type != actual.pp_var().type) {
    if (!expected.is_number() && !actual.is_number())
      return false;
  }
  // TODO(raymes): Implement a pp::Var::IsRefCounted() function.
  if (expected.pp_var().type > PP_VARTYPE_DOUBLE) {
    std::map<int64_t, int64_t>::const_iterator it =
        visited_ids->find(expected.pp_var().value.as_id);
    if (it != visited_ids->end()) {
      if (it->second == actual.pp_var().value.as_id)
        return true;
      return false;
    }
    // Round-tripping reference graphs with strings will not necessarily
    // result in isomorphic graphs. This is because string vars are converted
    // to string primitives in JS which cannot be referenced.
    if (!expected.is_string()) {
      (*visited_ids)[expected.pp_var().value.as_id] =
          actual.pp_var().value.as_id;
    }
  }

  if (expected.is_number()) {
    return fabs(expected.AsDouble() - actual.AsDouble()) < 1.0e-4;
  } else if (expected.is_array()) {
    pp::VarArray expected_array(expected);
    pp::VarArray actual_array(actual);
    if (expected_array.GetLength() != actual_array.GetLength())
      return false;
    for (uint32_t i = 0; i < expected_array.GetLength(); ++i) {
      if (!VarsEqual(expected_array.Get(i), actual_array.Get(i), visited_ids))
        return false;
    }
    return true;
  } else if (expected.is_dictionary()) {
    pp::VarDictionary expected_dict(expected);
    pp::VarDictionary actual_dict(actual);
    if (expected_dict.GetKeys().GetLength() !=
        actual_dict.GetKeys().GetLength()) {
      return false;
    }
    for (uint32_t i = 0; i < expected_dict.GetKeys().GetLength(); ++i) {
      pp::Var key = expected_dict.GetKeys().Get(i);
      if (!actual_dict.HasKey(key))
        return false;
      if (!VarsEqual(expected_dict.Get(key), actual_dict.Get(key), visited_ids))
        return false;
    }
    return true;
  } else {
    return expected == actual;
  }
}

bool VarsEqual(const pp::Var& expected,
               const pp::Var& actual) {
  std::map<int64_t, int64_t> visited_ids;
  return VarsEqual(expected, actual, &visited_ids);
}

#define FINISHED_WAITING_MESSAGE "TEST_POST_MESSAGE_FINISHED_WAITING"

}  // namespace

TestPostMessage::TestPostMessage(TestingInstance* instance)
    : TestCase(instance) {
}

TestPostMessage::~TestPostMessage() {
  instance_->PostMessage(pp::Var("This isn't guaranteed to be received, but "
                                 "shouldn't cause a crash."));

  // Remove the special listener that only responds to a FINISHED_WAITING
  // string. See Init for where it gets added.
  std::string js_code;
  js_code += "var plugin = document.getElementById('plugin');"
             "plugin.removeEventListener('message',"
             "                           plugin.wait_for_messages_handler);"
             "delete plugin.wait_for_messages_handler;";
  instance_->EvalScript(js_code);
}

bool TestPostMessage::Init() {
  bool success = CheckTestingInterface();

  // Add a post condition to tests which caches the postMessage function and
  // then calls it after the instance is destroyed. The ensures that no UAF
  // occurs because the MessageChannel may still be alive after the plugin
  // instance is destroyed (it will get garbage collected eventually).
  instance_->EvalScript("window.pluginPostMessage = "
                        "document.getElementById('plugin').postMessage");
  instance_->AddPostCondition("window.pluginPostMessage('') === undefined");

  // Set up a special listener that only responds to a FINISHED_WAITING string.
  // This is for use by WaitForMessages.
  std::string js_code;
  // Note the following code is dependent on some features of test_case.html.
  // E.g., it is assumed that the DOM element where the plugin is embedded has
  // an id of 'plugin', and there is a function 'IsTestingMessage' that allows
  // us to ignore the messages that are intended for use by the testing
  // framework itself.
  js_code += "var plugin = document.getElementById('plugin');"
             "var wait_for_messages_handler = function(message_event) {"
             "  if (!IsTestingMessage(message_event.data) &&"
             "      message_event.data === '" FINISHED_WAITING_MESSAGE "') {"
             "    plugin.postMessage('" FINISHED_WAITING_MESSAGE "');"
             "  }"
             "};"
             "plugin.addEventListener('message', wait_for_messages_handler);"
             // Stash it on the plugin so we can remove it in the destructor.
             "plugin.wait_for_messages_handler = wait_for_messages_handler;";
  instance_->EvalScript(js_code);

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  success = success && AddEchoingListener("message_event.data");
  message_data_.clear();
  // Send a message that the first test will expect to receive. This is to
  // verify that we can send messages when the 'Instance::Init' function is on
  // the stack.
  instance_->PostMessage(pp::Var(kTestString));

  return success;
}

void TestPostMessage::RunTests(const std::string& filter) {
  // Note: SendInInit must be first, because it expects to receive a message
  // that was sent in Init above.
  RUN_TEST(SendInInit, filter);
  RUN_TEST(SendingData, filter);
  RUN_TEST(SendingString, filter);
  RUN_TEST(SendingArrayBuffer, filter);
  RUN_TEST(SendingArray, filter);
  RUN_TEST(SendingDictionary, filter);
  RUN_TEST(SendingResource, filter);
  RUN_TEST(SendingComplexVar, filter);
  RUN_TEST(MessageEvent, filter);
  RUN_TEST(NoHandler, filter);
  RUN_TEST(ExtraParam, filter);
  if (testing_interface_->IsOutOfProcess())
    RUN_TEST(NonMainThread, filter);
}

void TestPostMessage::HandleMessage(const pp::Var& message_data) {
  if (message_data.is_string() &&
      (message_data.AsString() == FINISHED_WAITING_MESSAGE))
    testing_interface_->QuitMessageLoop(instance_->pp_instance());
  else
    message_data_.push_back(message_data);
}

bool TestPostMessage::AddEchoingListener(const std::string& expression) {
  std::string js_code;
  // Note the following code is dependent on some features of test_case.html.
  // E.g., it is assumed that the DOM element where the plugin is embedded has
  // an id of 'plugin', and there is a function 'IsTestingMessage' that allows
  // us to ignore the messages that are intended for use by the testing
  // framework itself.
  js_code += "var plugin = document.getElementById('plugin');"
             "var message_handler = function(message_event) {"
             "  if (!IsTestingMessage(message_event.data) &&"
             "      !(message_event.data === '" FINISHED_WAITING_MESSAGE "')) {"
             "    plugin.postMessage(";
  js_code += expression;
  js_code += "                      );"
             "  }"
             "};"
             "plugin.addEventListener('message', message_handler);"
             // Maintain an array of all event listeners, attached to the
             // plugin. This is so that we can easily remove them later (see
             // ClearListeners()).
             "if (!plugin.eventListeners) plugin.eventListeners = [];"
             "plugin.eventListeners.push(message_handler);";
  instance_->EvalScript(js_code);
  return true;
}

bool TestPostMessage::PostMessageFromJavaScript(const std::string& func) {
  std::string js_code;
  js_code += "var plugin = document.getElementById('plugin');"
             "plugin.postMessage(";
  js_code += func + "()";
  js_code += "                      );";
  instance_->EvalScript(js_code);
  return true;
}

bool TestPostMessage::ClearListeners() {
  std::string js_code;
  js_code += "var plugin = document.getElementById('plugin');"
             "while (plugin.eventListeners.length) {"
             "  plugin.removeEventListener('message',"
             "                             plugin.eventListeners.pop());"
             "}";
  instance_->EvalScript(js_code);
  return true;
}

int TestPostMessage::WaitForMessages() {
  size_t message_size_before = message_data_.size();
  // We first post a FINISHED_WAITING_MESSAGE. This should be guaranteed to
  // come back _after_ any other incoming messages that were already pending.
  instance_->PostMessage(pp::Var(FINISHED_WAITING_MESSAGE));
  testing_interface_->RunMessageLoop(instance_->pp_instance());
  // Now that the FINISHED_WAITING_MESSAGE has been echoed back to us, we know
  // that all pending messages have been slurped up. Return the number we
  // received (which may be zero).
  return static_cast<int>(message_data_.size() - message_size_before);
}

std::string TestPostMessage::CheckMessageProperties(
    const pp::Var& test_data,
    const std::vector<std::string>& properties_to_check) {
  typedef std::vector<std::string>::const_iterator Iterator;
  for (Iterator iter = properties_to_check.begin();
       iter != properties_to_check.end();
       ++iter) {
    ASSERT_TRUE(AddEchoingListener(*iter));
    message_data_.clear();
    instance_->PostMessage(test_data);
    ASSERT_EQ(0, message_data_.size());
    ASSERT_EQ(1, WaitForMessages());
    ASSERT_TRUE(message_data_.back().is_bool());
    if (!message_data_.back().AsBool())
      return std::string("Failed: ") + *iter;
    ASSERT_TRUE(message_data_.back().AsBool());
    ASSERT_TRUE(ClearListeners());
  }
  PASS();
}

std::string TestPostMessage::TestSendInInit() {
  // Wait for the messages from Init() to be guaranteed to be sent.
  WaitForMessages();
  // This test assumes Init already sent a message.
  ASSERT_EQ(1, message_data_.size());
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(kTestString, message_data_.back().AsString());
  message_data_.clear();
  PASS();
}

std::string TestPostMessage::TestSendingData() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));

  // Test sending a message to JavaScript for each supported type. The JS sends
  // the data back to us, and we check that they match.
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestBool));
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_EQ(message_data_.back().AsBool(), kTestBool);

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(static_cast<double>(kTestInt),
                   message_data_.back().AsDouble());

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestDouble));
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_number());
  ASSERT_DOUBLE_EQ(message_data_.back().AsDouble(), kTestDouble);

  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_undefined());

  message_data_.clear();
  instance_->PostMessage(pp::Var(pp::Var::Null()));
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_null());

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestSendingString() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  // Test that a string var is converted to a primitive JS string.
  message_data_.clear();
  std::vector<std::string> properties_to_check;
  properties_to_check.push_back(
      "typeof message_event.data === 'string'");
  ASSERT_SUBTEST_SUCCESS(CheckMessageProperties(kTestString,
                                                properties_to_check));

  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestString));
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), kTestString);

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestSendingArrayBuffer() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  // TODO(sehr,dmichael): Add testing of longer array buffers when
  // crbug.com/110086 is fixed.
  ScopedArrayBufferSizeSetter setter(testing_interface_,
                                     instance_->pp_instance(),
                                     200);
  uint32_t sizes[] = { 0, 100, 1000, 10000 };
  for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[i]); ++i) {
    std::ostringstream size_stream;
    size_stream << sizes[i];
    const std::string kSizeAsString(size_stream.str());

    // Create an appropriately sized array buffer with test_data[i] == i.
    pp::VarArrayBuffer test_data(sizes[i]);
    if (sizes[i] > 0)
      ASSERT_NE(NULL, test_data.Map());
    // Make sure we can Unmap/Map successfully (there's not really any way to
    // detect if it's unmapped, so we just re-map before getting the pointer to
    // the buffer).
    test_data.Unmap();
    test_data.Map();
    ASSERT_EQ(sizes[i], test_data.ByteLength());
    unsigned char* buff = static_cast<unsigned char*>(test_data.Map());
    const uint32_t kByteLength = test_data.ByteLength();
    for (size_t j = 0; j < kByteLength; ++j)
      buff[j] = static_cast<uint8_t>(j % 256u);

    // Have the listener test some properties of the ArrayBuffer.
    std::vector<std::string> properties_to_check;
    properties_to_check.push_back(
        "message_event.data.constructor.name === 'ArrayBuffer'");
    properties_to_check.push_back(
        std::string("message_event.data.byteLength === ") + kSizeAsString);
    if (sizes[i] > 0) {
      properties_to_check.push_back(
          "(new DataView(message_event.data)).getUint8(0) == 0");
      // Checks that the last element has the right value: (byteLength-1)%256.
      std::string received_byte("(new DataView(message_event.data)).getUint8("
                                "    message_event.data.byteLength-1)");
      std::string expected_byte("(message_event.data.byteLength-1)%256");
      properties_to_check.push_back(received_byte + " == " + expected_byte);
    }
    ASSERT_SUBTEST_SUCCESS(CheckMessageProperties(test_data,
                                                  properties_to_check));

    // Set up the JavaScript message event listener to echo the data part of the
    // message event back to us.
    ASSERT_TRUE(AddEchoingListener("message_event.data"));
    message_data_.clear();
    instance_->PostMessage(test_data);
    // PostMessage is asynchronous, so we should not receive a response yet.
    ASSERT_EQ(0, message_data_.size());
    ASSERT_EQ(1, WaitForMessages());
    ASSERT_TRUE(message_data_.back().is_array_buffer());
    pp::VarArrayBuffer received(message_data_.back());
    message_data_.clear();
    ASSERT_EQ(test_data.ByteLength(), received.ByteLength());
    unsigned char* received_buff = static_cast<unsigned char*>(received.Map());
    // The buffer should be copied, so this should be a distinct buffer. When
    // 'transferrables' are implemented for PPAPI, we'll also want to test that
    // we get the _same_ buffer back when it's transferred.
    if (sizes[i] > 0)
      ASSERT_NE(buff, received_buff);
    for (size_t byte = 0; byte < test_data.ByteLength(); ++byte)
      ASSERT_EQ(buff[byte], received_buff[byte]);

    message_data_.clear();
    ASSERT_TRUE(ClearListeners());
  }

  PASS();
}

std::string TestPostMessage::TestSendingArray() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  pp::VarArray array;
  array.Set(0, pp::Var(kTestBool));
  array.Set(1, pp::Var(kTestString));
  // Purposely leave index 2 empty.
  array.Set(3, pp::Var(kTestInt));
  array.Set(4, pp::Var(kTestDouble));

  std::stringstream ss;
  ss << array.GetLength();
  std::string length_as_string(ss.str());

  // Have the listener test some properties of the Array.
  std::vector<std::string> properties_to_check;
  properties_to_check.push_back(
      "message_event.data.constructor.name === 'Array'");
  properties_to_check.push_back(
      std::string("message_event.data.length === ") + length_as_string);
  // Check that the string is converted to a primitive JS string.
  properties_to_check.push_back(
      std::string("typeof message_event.data[1] === 'string'"));
  ASSERT_SUBTEST_SUCCESS(CheckMessageProperties(array, properties_to_check));

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  message_data_.clear();
  instance_->PostMessage(array);
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_array());
  ASSERT_TRUE(VarsEqual(array, message_data_.back()));

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestSendingDictionary() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  pp::VarDictionary dictionary;
  dictionary.Set(pp::Var("foo"), pp::Var(kTestBool));
  dictionary.Set(pp::Var("bar"), pp::Var(kTestString));
  dictionary.Set(pp::Var("abc"), pp::Var(kTestInt));
  dictionary.Set(pp::Var("def"), pp::Var());

  std::stringstream ss;
  ss << dictionary.GetKeys().GetLength();
  std::string length_as_string(ss.str());

  // Have the listener test some properties of the Dictionary.
  std::vector<std::string> properties_to_check;
  properties_to_check.push_back(
      "message_event.data.constructor.name === 'Object'");
  properties_to_check.push_back(
      std::string("Object.keys(message_event.data).length === ") +
      length_as_string);
  // Check that the string is converted to a primitive JS string.
  properties_to_check.push_back(
      std::string("typeof message_event.data['bar'] === 'string'"));
  ASSERT_SUBTEST_SUCCESS(CheckMessageProperties(dictionary,
                                                properties_to_check));

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  message_data_.clear();
  instance_->PostMessage(dictionary);
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_dictionary());
  ASSERT_TRUE(VarsEqual(dictionary, message_data_.back()));

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestSendingResource() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  std::string file_path("/");
  file_path += kTestFilename;
  int content_length = static_cast<int>(strlen(kTestString));

  // Create a file in the HTML5 temporary file system, in the Pepper plugin.
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  pp::FileSystem file_system(instance_, PP_FILESYSTEMTYPE_LOCALTEMPORARY);
  callback.WaitForResult(file_system.Open(1024, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  pp::FileRef write_file_ref(file_system, file_path.c_str());
  // Write to the file.
  pp::FileIO write_file_io(instance_);
  ASSERT_NE(0, write_file_io.pp_resource());
  callback.WaitForResult(
      write_file_io.Open(write_file_ref,
                         PP_FILEOPENFLAG_WRITE | PP_FILEOPENFLAG_CREATE,
                         callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  callback.WaitForResult(write_file_io.Write(
      0, kTestString, content_length, callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(callback.result(), content_length);
  write_file_io.Close();

  // Pass the file system to JavaScript and have the listener test some
  // properties of the file system.
  pp::Var file_system_var(file_system);
  std::vector<std::string> properties_to_check;
  properties_to_check.push_back("message_event.data.root.isDirectory");
  properties_to_check.push_back(
      "message_event.data.name.indexOf("
      "    ':Temporary',"
      "    message_event.data.name.length - ':Temporary'.length) !== -1");
  ASSERT_SUBTEST_SUCCESS(CheckMessageProperties(file_system_var,
                                                properties_to_check));

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  // Send the file system in a message from the Pepper plugin to JavaScript.
  message_data_.clear();
  instance_->PostMessage(file_system_var);
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());

  // The JavaScript should have posted the file system back to us. Verify that
  // it is a file system and read the file contents that we wrote earlier.
  pp::Var var = message_data_.back();
  ASSERT_TRUE(var.is_resource());
  pp::Resource result = var.AsResource();
  ASSERT_TRUE(pp::FileSystem::IsFileSystem(result));
  {
    pp::FileSystem received_file_system(result);
    pp::FileRef file_ref(received_file_system, file_path.c_str());
    ASSERT_NE(0, file_ref.pp_resource());

    // Ensure that the file can be queried.
    TestCompletionCallbackWithOutput<PP_FileInfo> cc(instance_->pp_instance(),
                                                     callback_type());
    cc.WaitForResult(file_ref.Query(cc.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(cc);
    ASSERT_EQ(PP_OK, cc.result());
    ASSERT_EQ(cc.output().size, content_length);

    // Read the file and test that its contents match.
    pp::FileIO file_io(instance_);
    ASSERT_NE(0, file_io.pp_resource());
    callback.WaitForResult(
        file_io.Open(file_ref, PP_FILEOPENFLAG_READ, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(PP_OK, callback.result());

    std::vector<char> buffer_vector(content_length);
    char* buffer = &buffer_vector[0];  // Note: Not null-terminated!
    callback.WaitForResult(
        file_io.Read(0, buffer, content_length, callback.GetCallback()));
    CHECK_CALLBACK_BEHAVIOR(callback);
    ASSERT_EQ(callback.result(), content_length);
    ASSERT_EQ(0, memcmp(buffer, kTestString, content_length));
  }

  WaitForMessages();
  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestSendingComplexVar() {
  // Clean up after previous tests. This also swallows the message sent by Init
  // if we didn't run the 'SendInInit' test. All tests other than 'SendInInit'
  // should start with these.
  WaitForMessages();
  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  pp::Var string(kTestString);
  pp::VarDictionary dictionary;
  dictionary.Set(pp::Var("foo"), pp::Var(kTestBool));
  dictionary.Set(pp::Var("bar"), string);
  dictionary.Set(pp::Var("abc"), pp::Var(kTestInt));
  dictionary.Set(pp::Var("def"), pp::Var());

  // Reference to array.
  pp::VarArray array;
  array.Set(0, pp::Var(kTestBool));
  array.Set(1, string);
  // Purposely leave index 2 empty (which will place an undefined var there).
  array.Set(3, pp::Var(kTestInt));
  array.Set(4, pp::Var(kTestDouble));

  dictionary.Set(pp::Var("array-ref1"), array);
  dictionary.Set(pp::Var("array-ref2"), array);

  // Set up the JavaScript message event listener to echo the data part of the
  // message event back to us.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  instance_->PostMessage(dictionary);
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_dictionary());
  pp::VarDictionary result(message_data_.back());
  ASSERT_TRUE(VarsEqual(dictionary, message_data_.back()));

  WaitForMessages();
  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  // Set up a (dictionary -> array -> dictionary) cycle. Cycles shouldn't be
  // transmitted.
  pp::VarArray array2;
  array2.Set(0, dictionary);
  dictionary.Set(pp::Var("array2"), array2);

  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  instance_->PostMessage(dictionary);
  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(WaitForMessages(), 0);

  // Break the cycles.
  dictionary.Delete(pp::Var("array2"));

  WaitForMessages();
  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  // Test sending a cycle from JavaScript to the plugin.
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  PostMessageFromJavaScript("function() { var x = []; x[0] = x; return x; }");
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(WaitForMessages(), 0);

  WaitForMessages();
  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestMessageEvent() {
  // Set up the JavaScript message event listener to pass us some values from
  // the MessageEvent and make sure they match our expectations.

  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  // Have the listener pass back the class name of message_event and make sure
  // it's "MessageEvent".
  ASSERT_TRUE(AddEchoingListener("message_event.constructor.name"));
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_string());
  ASSERT_EQ(message_data_.back().AsString(), "MessageEvent");
  ASSERT_TRUE(ClearListeners());

  // Make sure all the non-data properties have the expected values.
  bool success = AddEchoingListener("((message_event.origin === '')"
                                   " && (message_event.lastEventId === '')"
                                   " && (message_event.source === null)"
                                   " && (message_event.ports.length === 0)"
                                   " && (message_event.bubbles === false)"
                                   " && (message_event.cancelable === false)"
                                   ")");
  ASSERT_TRUE(success);
  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  ASSERT_EQ(0, message_data_.size());
  ASSERT_EQ(1, WaitForMessages());
  ASSERT_TRUE(message_data_.back().is_bool());
  ASSERT_TRUE(message_data_.back().AsBool());
  ASSERT_TRUE(ClearListeners());

  // Add some event handlers to make sure they receive messages.
  ASSERT_TRUE(AddEchoingListener("1"));
  ASSERT_TRUE(AddEchoingListener("2"));
  ASSERT_TRUE(AddEchoingListener("3"));

  message_data_.clear();
  instance_->PostMessage(pp::Var(kTestInt));
  // Make sure we don't get a response in a re-entrant fashion.
  ASSERT_EQ(0, message_data_.size());
  // We should get 3 messages.
  ASSERT_EQ(WaitForMessages(), 3);
  // Copy to a vector of doubles and sort; w3c does not specify the order for
  // event listeners. (Copying is easier than writing an operator< for pp::Var.)
  //
  // See http://www.w3.org/TR/2000/REC-DOM-Level-2-Events-20001113/events.html.
  VarVector::iterator iter(message_data_.begin()), the_end(message_data_.end());
  std::vector<double> double_vec;
  for (; iter != the_end; ++iter) {
    ASSERT_TRUE(iter->is_number());
    double_vec.push_back(iter->AsDouble());
  }
  std::sort(double_vec.begin(), double_vec.end());
  ASSERT_DOUBLE_EQ(double_vec[0], 1.0);
  ASSERT_DOUBLE_EQ(double_vec[1], 2.0);
  ASSERT_DOUBLE_EQ(double_vec[2], 3.0);

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestNoHandler() {
  // Delete any lingering messages and event listeners.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());

  // Now send a message.  We shouldn't get a response.
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(WaitForMessages(), 0);
  ASSERT_TRUE(message_data_.empty());

  PASS();
}

std::string TestPostMessage::TestExtraParam() {
  // Delete any lingering messages and event listeners.
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  // Add a listener that will respond with 1 and an empty array (where the
  // message port array would appear if it was Worker postMessage).
  ASSERT_TRUE(AddEchoingListener("1, []"));

  // Now send a message.  We shouldn't get a response.
  message_data_.clear();
  instance_->PostMessage(pp::Var());
  ASSERT_EQ(WaitForMessages(), 0);
  ASSERT_TRUE(message_data_.empty());

  ASSERT_TRUE(ClearListeners());

  PASS();
}

std::string TestPostMessage::TestNonMainThread() {
  WaitForMessages();
  ASSERT_TRUE(ClearListeners());
  ASSERT_TRUE(AddEchoingListener("message_event.data"));
  message_data_.clear();

  // Set up a thread for each integer from 0 to (kThreadsToRun - 1).  Make each
  // thread send the number that matches its index kMessagesToSendPerThread
  // times.  For good measure, call postMessage from the main thread
  // kMessagesToSendPerThread times. At the end, we make sure we got all the
  // values we expected.
  PP_Thread threads[kThreadsToRun];
  for (int32_t i = 0; i < kThreadsToRun; ++i) {
    // Set up a thread to send a value of i.
    void* arg = new InvokePostMessageThreadArg(instance_, pp::Var(i));
    PP_CreateThread(&threads[i], &InvokePostMessageThreadFunc, arg);
  }
  // Invoke PostMessage right now to send a value of (kThreadsToRun).
  for (int32_t i = 0; i < kMessagesToSendPerThread; ++i)
    instance_->PostMessage(pp::Var(kThreadsToRun));

  // Now join all threads.
  for (int32_t i = 0; i < kThreadsToRun; ++i)
    PP_JoinThread(threads[i]);

  // PostMessage is asynchronous, so we should not receive a response yet.
  ASSERT_EQ(0, message_data_.size());

  // Make sure we got all values that we expected.  Note that because it's legal
  // for the JavaScript engine to treat our integers as floating points, we
  // can't just use std::find or equality comparison. So we instead, we convert
  // each incoming value to an integer, and count them in received_counts.
  int32_t expected_num = (kThreadsToRun + 1) * kMessagesToSendPerThread;
  // Count how many we receive per-index.
  std::vector<int32_t> expected_counts(kThreadsToRun + 1,
                                       kMessagesToSendPerThread);
  std::vector<int32_t> received_counts(kThreadsToRun + 1, 0);
  ASSERT_EQ(expected_num, WaitForMessages());
  for (int32_t i = 0; i < expected_num; ++i) {
    const pp::Var& latest_var(message_data_[i]);
    ASSERT_TRUE(latest_var.is_int() || latest_var.is_double());
    int32_t received_value = -1;
    if (latest_var.is_int()) {
      received_value = latest_var.AsInt();
    } else if (latest_var.is_double()) {
      received_value = static_cast<int32_t>(latest_var.AsDouble() + 0.5);
    }
    ASSERT_TRUE(received_value >= 0);
    ASSERT_TRUE(received_value <= kThreadsToRun);
    ++received_counts[received_value];
  }
  ASSERT_EQ(expected_counts, received_counts);

  message_data_.clear();
  ASSERT_TRUE(ClearListeners());

  PASS();
}
