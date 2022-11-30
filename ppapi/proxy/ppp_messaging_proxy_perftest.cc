// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/perf_time_logger.h"
#include "ppapi/c/ppp_messaging.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {
namespace proxy {
namespace {

base::WaitableEvent handle_message_called(
    base::WaitableEvent::ResetPolicy::AUTOMATIC,
    base::WaitableEvent::InitialState::NOT_SIGNALED);

void HandleMessage(PP_Instance /* instance */, PP_Var message_data) {
  ppapi::ProxyAutoLock lock;
  StringVar* string_var = StringVar::FromPPVar(message_data);
  DCHECK(string_var);
  // Retrieve the string to make sure the proxy can't "optimize away" sending
  // the actual contents of the string (e.g., by doing a lazy retrieve or
  // something). Note that this test is for performance only, and assumes
  // other tests check for correctness.
  std::string s = string_var->value();
  // Do something simple with the string so the compiler won't complain.
  if (s.length() > 0)
    s[0] = 'a';
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(message_data);
  handle_message_called.Signal();
}

PPP_Messaging ppp_messaging_mock = {
  &HandleMessage
};

class PppMessagingPerfTest : public TwoWayTest {
 public:
  PppMessagingPerfTest() : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
    plugin().RegisterTestInterface(PPP_MESSAGING_INTERFACE,
                                   &ppp_messaging_mock);
  }
};

}  // namespace

// Tests the performance of sending strings through the proxy.
TEST_F(PppMessagingPerfTest, StringPerformance) {
  const PP_Instance kTestInstance = pp_instance();
  int seed = 123;
  int string_count = 1000;
  int max_string_size = 1000000;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line) {
    if (command_line->HasSwitch("seed")) {
      base::StringToInt(command_line->GetSwitchValueASCII("seed"),
                        &seed);
    }
    if (command_line->HasSwitch("string_count")) {
      base::StringToInt(command_line->GetSwitchValueASCII("string_count"),
                        &string_count);
    }
    if (command_line->HasSwitch("max_string_size")) {
      base::StringToInt(command_line->GetSwitchValueASCII("max_string_size"),
                        &max_string_size);
    }
  }
  srand(seed);
  base::PerfTimeLogger logger("PppMessagingPerfTest.StringPerformance");
  for (int i = 0; i < string_count; ++i) {
    const std::string test_string(rand() % max_string_size, 'a');
    PP_Var host_string = StringVar::StringToPPVar(test_string);
    // We don't have a host-side PPP_Messaging interface; just send the message
    // directly like the proxy does.
    host().host_dispatcher()->Send(new PpapiMsg_PPPMessaging_HandleMessage(
        ppapi::API_ID_PPP_MESSAGING,
        kTestInstance,
        ppapi::proxy::SerializedVarSendInput(host().host_dispatcher(),
                                             host_string)));
    handle_message_called.Wait();
    PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(host_string);
  }
}

}  // namespace proxy
}  // namespace ppapi

