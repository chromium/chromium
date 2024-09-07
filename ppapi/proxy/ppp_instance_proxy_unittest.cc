// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/synchronization/waitable_event.h"
#include "ipc/ipc_message_utils.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/c/ppb_url_loader.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/ppb_view_shared.h"

namespace ppapi {
namespace proxy {

namespace {

// This is an ad-hoc mock of PPP_Instance using global variables. Eventually,
// generalize making PPAPI interface mocks by using IDL or macro/template magic.
PP_Instance received_instance;
uint32_t received_argc;
std::vector<std::string> received_argn;
std::vector<std::string> received_argv;
PP_Bool bool_to_return;
PP_Bool DidCreate(PP_Instance instance, uint32_t argc, const char* argn[],
                  const char* argv[]) {
  received_instance = instance;
  received_argc = argc;
  received_argn.clear();
  received_argn.insert(received_argn.begin(), argn, argn + argc);
  received_argv.clear();
  received_argv.insert(received_argv.begin(), argv, argv + argc);
  return bool_to_return;
}

void DidDestroy(PP_Instance instance) {
  received_instance = instance;
}

PP_Rect received_position;
PP_Rect received_clip;
// DidChangeView is asynchronous. We wait until the call has completed before
// proceeding on to the next test.
base::WaitableEvent did_change_view_called(
    base::WaitableEvent::ResetPolicy::AUTOMATIC,
    base::WaitableEvent::InitialState::NOT_SIGNALED);
void DidChangeView(PP_Instance instance, const PP_Rect* position,
                   const PP_Rect* clip) {
  received_instance = instance;
  received_position = *position;
  received_clip = *clip;
  did_change_view_called.Signal();
}

PP_Bool received_has_focus;
base::WaitableEvent did_change_focus_called(
    base::WaitableEvent::ResetPolicy::AUTOMATIC,
    base::WaitableEvent::InitialState::NOT_SIGNALED);
void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
  received_instance = instance;
  received_has_focus = has_focus;
  did_change_focus_called.Signal();
}

PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader) {
  // This one requires use of the PPB_URLLoader proxy and PPB_Core, plus a
  // resource tracker for the url_loader resource.
  // TODO(dmichael): Mock those out and test this function.
  NOTREACHED();
}

// Clear all the 'received' values for our mock.  Call this before you expect
// one of the functions to be invoked.  TODO(dmichael): It would be better to
// have a flag also for each function, so we know the right one got called.
void ResetReceived() {
  received_instance = 0;
  received_argc = 0;
  received_argn.clear();
  received_argv.clear();
  memset(&received_position, 0, sizeof(received_position));
  memset(&received_clip, 0, sizeof(received_clip));
  received_has_focus = PP_FALSE;
}

PPP_Instance_1_0 ppp_instance_1_0 = {
  &DidCreate,
  &DidDestroy,
  &DidChangeView,
  &DidChangeFocus,
  &HandleDocumentLoad
};

// PPP_Instance_Proxy::DidChangeView relies on PPB_Fullscreen being
// available with a valid implementation of IsFullScreen, so we mock it.
PP_Bool IsFullscreen(PP_Instance instance) {
  return PP_FALSE;
}
PPB_Fullscreen ppb_fullscreen = { &IsFullscreen };

}  // namespace

class PPP_Instance_ProxyTest : public TwoWayTest {
 public:
   PPP_Instance_ProxyTest()
       : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
   }
};

TEST_F(PPP_Instance_ProxyTest, PPPInstance1_0) {
  plugin().RegisterTestInterface(PPP_INSTANCE_INTERFACE_1_0, &ppp_instance_1_0);
  host().RegisterTestInterface(PPB_FULLSCREEN_INTERFACE,
                               &ppb_fullscreen);

  // Grab the host-side proxy for the interface. The browser only speaks 1.1,
  // while the proxy ensures support for the 1.0 version on the plugin side.
  const PPP_Instance_1_1* ppp_instance = static_cast<const PPP_Instance_1_1*>(
      host().host_dispatcher()->GetProxiedInterface(
          PPP_INSTANCE_INTERFACE_1_1));

  // Call each function in turn, make sure we get the expected values and
  // returns.
  //
  // We don't test DidDestroy, because it has the side-effect of removing the
  // PP_Instance from the PluginDispatcher, which will cause a failure later
  // when the test is torn down.
  PP_Instance expected_instance = pp_instance();
  std::vector<std::string> expected_argn, expected_argv;
  expected_argn.push_back("Hello");
  expected_argn.push_back("world.");
  expected_argv.push_back("elloHay");
  expected_argv.push_back("orldway.");
  std::vector<const char*> argn_to_pass, argv_to_pass;
  CHECK(expected_argn.size() == expected_argv.size());
  for (size_t i = 0; i < expected_argn.size(); ++i) {
    argn_to_pass.push_back(expected_argn[i].c_str());
    argv_to_pass.push_back(expected_argv[i].c_str());
  }
  uint32_t expected_argc = static_cast<uint32_t>(expected_argn.size());
  bool_to_return = PP_TRUE;
  ResetReceived();
  // Tell the host resource tracker about the instance.
  host().resource_tracker().DidCreateInstance(expected_instance);
  EXPECT_EQ(bool_to_return, ppp_instance->DidCreate(expected_instance,
                                                    expected_argc,
                                                    &argn_to_pass[0],
                                                    &argv_to_pass[0]));
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_argc, expected_argc);
  EXPECT_EQ(received_argn, expected_argn);
  EXPECT_EQ(received_argv, expected_argv);

  PP_Rect expected_position = { {1, 2}, {3, 4} };
  PP_Rect expected_clip = { {5, 6}, {7, 8} };
  ViewData data;
  data.rect = expected_position;
  data.is_fullscreen = false;
  data.is_page_visible = true;
  data.clip_rect = expected_clip;
  data.device_scale = 1.0f;
  ResetReceived();
  LockingResourceReleaser view_resource(
      (new PPB_View_Shared(OBJECT_IS_IMPL,
                           expected_instance, data))->GetReference());
  ppp_instance->DidChangeView(expected_instance, view_resource.get());
  did_change_view_called.Wait();
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_position.point.x, expected_position.point.x);
  EXPECT_EQ(received_position.point.y, expected_position.point.y);
  EXPECT_EQ(received_position.size.width, expected_position.size.width);
  EXPECT_EQ(received_position.size.height, expected_position.size.height);
  EXPECT_EQ(received_clip.point.x, expected_clip.point.x);
  EXPECT_EQ(received_clip.point.y, expected_clip.point.y);
  EXPECT_EQ(received_clip.size.width, expected_clip.size.width);
  EXPECT_EQ(received_clip.size.height, expected_clip.size.height);

  PP_Bool expected_has_focus = PP_TRUE;
  ResetReceived();
  ppp_instance->DidChangeFocus(expected_instance, expected_has_focus);
  did_change_focus_called.Wait();
  EXPECT_EQ(received_instance, expected_instance);
  EXPECT_EQ(received_has_focus, expected_has_focus);

  //  TODO(dmichael): Need to mock out a resource Tracker to be able to test
  //                  HandleResourceLoad. It also requires
  //                  PPB_Core.AddRefResource and for PPB_URLLoader to be
  //                  registered.

  host().resource_tracker().DidDeleteInstance(expected_instance);
}

}  // namespace proxy
}  // namespace ppapi
