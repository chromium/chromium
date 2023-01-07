// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_POST_MESSAGE_H_
#define PPAPI_TESTS_TEST_POST_MESSAGE_H_

#include <string>
#include <vector>

#include "ppapi/tests/test_case.h"

class TestPostMessage : public TestCase {
 public:
  explicit TestPostMessage(TestingInstance* instance);
  virtual ~TestPostMessage();

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  // A handler for JS->Native calls to postMessage.  Simply pushes
  // the given value to the back of message_data_
  virtual void HandleMessage(const pp::Var& message_data);

  // Add a listener for message events which will echo back the given
  // JavaScript expression by passing it to postMessage. JavaScript Variables
  // available to the expression are:
  //  'plugin' - the DOM element for the test plugin.
  //  'message_event' - the message event parameter to the listener function.
  // This also adds the new listener to an array called 'eventListeners' on the
  // plugin's DOM element. This is used by ClearListeners().
  // Returns true on success, false on failure.
  bool AddEchoingListener(const std::string& expression);

  // Posts a message from JavaScript to the plugin. |func| should be a
  // JavaScript function which returns the variable to post.
  bool PostMessageFromJavaScript(const std::string& func);

  // Clear any listeners that have been added using AddEchoingListener by
  // calling removeEventListener for each.
  // Returns true on success, false on failure.
  bool ClearListeners();

  // Wait for pending messages; return the number of messages that were pending
  // at the time of invocation.
  int WaitForMessages();

  // Posts a message from JavaScript to the plugin and wait for it to arrive.
  // |func| should be a JavaScript function(callback) which calls |callback|
  // with the variable to post. This function will block until the message
  // arrives on the plugin side (there is no need to use WaitForMessages()).
  // Returns the number of messages that were pending at the time of invocation.
  int PostAsyncMessageFromJavaScriptAndWait(const std::string& func);

  // Verifies that the given javascript assertions are true of the message
  // (|test_data|) passed via PostMessage().
  std::string CheckMessageProperties(
      const pp::Var& test_data,
      const std::vector<std::string>& properties_to_check);

  // Test that we can send a message from Instance::Init. Note the actual
  // message is sent in TestPostMessage::Init, and this test simply makes sure
  // we got it.
  std::string TestSendInInit();

  // Test some basic functionality;  make sure we can send data successfully
  // in both directions.
  std::string TestSendingData();

  // Test sending string vars in both directions.
  std::string TestSendingString();

  // Test sending ArrayBuffer vars in both directions.
  std::string TestSendingArrayBuffer();

  // Test sending Array vars in both directions.
  std::string TestSendingArray();

  // Test sending Dictionary vars in both directions.
  std::string TestSendingDictionary();

  // Test sending Resource vars in both directions.
  std::string TestSendingResource();

  // Test sending a complex var with references and cycles in both directions.
  std::string TestSendingComplexVar();

  // Test the MessageEvent object that JavaScript received to make sure it is
  // of the right type and has all the expected fields.
  std::string TestMessageEvent();

  // Test sending a message when no handler exists, make sure nothing happens.
  std::string TestNoHandler();

  // Test sending from JavaScript to the plugin with extra parameters, make sure
  // nothing happens.
  std::string TestExtraParam();

  // Test sending messages off of the main thread.
  std::string TestNonMainThread();

  typedef std::vector<pp::Var> VarVector;

  // This is used to store pp::Var objects we receive via a call to
  // HandleMessage.
  VarVector message_data_;
};

#endif  // PPAPI_TESTS_TEST_POST_MESSAGE_H_

