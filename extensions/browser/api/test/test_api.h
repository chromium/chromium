// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_TEST_TEST_API_H_
#define EXTENSIONS_BROWSER_API_TEST_TEST_API_H_

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "extensions/browser/extension_function.h"

namespace base {

template <typename T>
struct DefaultSingletonTraits;

}  // namespace base

namespace extensions {

// A function that is only available in tests.
// Prior to running, checks that we are in a testing process.
class TestExtensionFunction : public ExtensionFunction {
 protected:
  ~TestExtensionFunction() override;

  // ExtensionFunction:
  bool PreRunValidation(std::string* error) override;
};

class TestNotifyPassFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.notifyPass", UNKNOWN)

 protected:
  ~TestNotifyPassFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TestNotifyFailFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.notifyFail", UNKNOWN)

 protected:
  ~TestNotifyFailFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TestOpenFileUrlFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.openFileUrl", UNKNOWN)

 protected:
  ~TestOpenFileUrlFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TestLogFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.log", UNKNOWN)

 protected:
  ~TestLogFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TestSendMessageFunction : public ExtensionFunction {
 public:
  TestSendMessageFunction();
  DECLARE_EXTENSION_FUNCTION("test.sendMessage", UNKNOWN)

  // Sends a reply back to the calling extension. Many extensions don't need
  // a reply and will just ignore it.
  void Reply(const std::string& message);

  // Sends an error back to the calling extension.
  void ReplyWithError(const std::string& error);

 protected:
  ~TestSendMessageFunction() override;

  // UIExtensionFunction:
  ResponseAction Run() override;

  // Whether or not the function is currently waiting for a reply.
  bool waiting_ = false;

  std::optional<ResponseValue> response_;
};

class TestSendScriptResultFunction : public TestExtensionFunction {
 public:
  TestSendScriptResultFunction();
  DECLARE_EXTENSION_FUNCTION("test.sendScriptResult", UNKNOWN)

 private:
  ~TestSendScriptResultFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TestGetConfigFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.getConfig", UNKNOWN)

  // Set the dictionary returned by chrome.test.getConfig().
  // Does not take ownership of |value|.
  static void set_test_config_state(base::Value::Dict* value);

 protected:
  // Tests that set configuration state do so by calling
  // set_test_config_state() as part of test set up, and unsetting it
  // during tear down.  This singleton class holds a pointer to that
  // state, owned by the test code.
  class TestConfigState {
   public:
    TestConfigState(const TestConfigState&) = delete;
    TestConfigState& operator=(const TestConfigState&) = delete;

    static TestConfigState* GetInstance();

    void set_config_state(base::Value::Dict* config_state) {
      config_state_ = config_state;
    }

    const base::Value::Dict* config_state() { return config_state_; }

   private:
    friend struct base::DefaultSingletonTraits<TestConfigState>;
    TestConfigState();

    raw_ptr<base::Value::Dict, DanglingUntriaged> config_state_;
  };

  ~TestGetConfigFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class TestWaitForRoundTripFunction : public TestExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("test.waitForRoundTrip", UNKNOWN)

 protected:
  ~TestWaitForRoundTripFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_TEST_TEST_API_H_
