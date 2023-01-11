// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstring>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/api_id.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {
namespace proxy {

namespace {

// This is an ad-hoc mock of PPP_Messaging using global variables. Eventually,
// generalize making PPAPI interface mocks by using IDL or macro/template magic.
PP_Instance g_received_instance;
PP_Var g_received_var;
base::WaitableEvent handle_message_called(
    base::WaitableEvent::ResetPolicy::AUTOMATIC,
    base::WaitableEvent::InitialState::NOT_SIGNALED);

void HandleMessage(PP_Instance instance, PP_Var message_data) {
  g_received_instance = instance;
  g_received_var = message_data;
  handle_message_called.Signal();
}

// Clear all the 'received' values for our mock.  Call this before you expect
// one of the functions to be invoked.
void ResetReceived() {
  g_received_instance = 0;
  g_received_var.type = PP_VARTYPE_UNDEFINED;
  g_received_var.value.as_id = 0;
}

PPP_Messaging ppp_messaging_mock = {
  &HandleMessage
};

// CallHandleMessage does the host-side work to cause HandleMessage to be called
// in the plugin side.
void CallHandleMessage(Dispatcher* dispatcher,
                       PP_Instance instance,
                       PP_Var message) {
  dispatcher->Send(new PpapiMsg_PPPMessaging_HandleMessage(
      API_ID_PPP_MESSAGING,
      instance,
      SerializedVarSendInputShmem(dispatcher, message, instance)));
}

class PPP_Messaging_ProxyTest : public TwoWayTest {
 public:
  PPP_Messaging_ProxyTest()
      : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
    plugin().RegisterTestInterface(PPP_MESSAGING_INTERFACE,
                                   &ppp_messaging_mock);
  }
};

void CompareAndReleaseStringVar(PluginProxyTestHarness* plugin_harness,
                                PP_Var received_var,
                                const std::string& test_string) {
  ProxyAutoLock lock;
  Var* received_string = plugin_harness->var_tracker().GetVar(received_var);
  ASSERT_TRUE(received_string);
  ASSERT_TRUE(received_string->AsStringVar());
  EXPECT_EQ(test_string, received_string->AsStringVar()->value());
  // Now release the var, and the string should go away (because the ref
  // count should be one).
  plugin_harness->var_tracker().ReleaseVar(received_var);
  EXPECT_FALSE(StringVar::FromPPVar(received_var));
}

}  // namespace

TEST_F(PPP_Messaging_ProxyTest, SendMessages) {
  PP_Instance expected_instance = pp_instance();
  PP_Var expected_var = PP_MakeUndefined();
  ResetReceived();
  Dispatcher* host_dispatcher = host().GetDispatcher();
  CallHandleMessage(host_dispatcher, expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, g_received_instance);
  EXPECT_EQ(expected_var.type, g_received_var.type);

  expected_var = PP_MakeNull();
  ResetReceived();
  CallHandleMessage(host_dispatcher, expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, g_received_instance);
  EXPECT_EQ(expected_var.type, g_received_var.type);

  expected_var = PP_MakeBool(PP_TRUE);
  ResetReceived();
  CallHandleMessage(host_dispatcher, expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, g_received_instance);
  EXPECT_EQ(expected_var.type, g_received_var.type);
  EXPECT_EQ(expected_var.value.as_bool, g_received_var.value.as_bool);

  expected_var = PP_MakeInt32(12345);
  ResetReceived();
  CallHandleMessage(host_dispatcher, expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, g_received_instance);
  EXPECT_EQ(expected_var.type, g_received_var.type);
  EXPECT_EQ(expected_var.value.as_int, g_received_var.value.as_int);

  expected_var = PP_MakeDouble(3.1415);
  ResetReceived();
  CallHandleMessage(host_dispatcher, expected_instance, expected_var);
  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, g_received_instance);
  EXPECT_EQ(expected_var.type, g_received_var.type);
  EXPECT_EQ(expected_var.value.as_double, g_received_var.value.as_double);

  const std::string kTestString("Hello world!");
  expected_var = StringVar::StringToPPVar(kTestString);
  ResetReceived();
  CallHandleMessage(host_dispatcher, expected_instance, expected_var);
  // Now release the var, and the string should go away (because the ref
  // count should be one).
  host().var_tracker().ReleaseVar(expected_var);
  EXPECT_FALSE(StringVar::FromPPVar(expected_var));

  handle_message_called.Wait();
  EXPECT_EQ(expected_instance, g_received_instance);
  EXPECT_EQ(expected_var.type, g_received_var.type);
  PostTaskOnRemoteHarness(base::BindOnce(CompareAndReleaseStringVar, &plugin(),
                                         g_received_var, kTestString));
}

}  // namespace proxy
}  // namespace ppapi

