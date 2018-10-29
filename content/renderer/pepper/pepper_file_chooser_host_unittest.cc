// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>
#include <tuple>

#include "base/files/file_path.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/pepper/mock_renderer_ppapi_host.h"
#include "content/renderer/pepper/pepper_file_chooser_host.h"
#include "content/renderer/render_view_impl.h"
#include "content/test/test_content_client.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_message_test_sink.h"
#include "ppapi/shared_impl/file_ref_create_info.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource_tracker.h"
#include "ppapi/shared_impl/test_globals.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class PepperFileChooserHostTest : public RenderViewTest {
 public:
  PepperFileChooserHostTest() : pp_instance_(123456) {}

  void SetUp() override {
    SetContentClient(&client_);
    RenderViewTest::SetUp();

    globals_.GetResourceTracker()->DidCreateInstance(pp_instance_);
  }
  void TearDown() override {
    globals_.GetResourceTracker()->DidDeleteInstance(pp_instance_);

    RenderViewTest::TearDown();
  }

  PP_Instance pp_instance() const { return pp_instance_; }

 private:
  PP_Instance pp_instance_;

  // Disables locking for the duration of the test.
  ppapi::ProxyLock::LockingDisablerForTest disable_locking_;
  ppapi::TestGlobals globals_;
  TestContentClient client_;
};

}  // namespace

TEST_F(PepperFileChooserHostTest, Show) {
  PP_Resource pp_resource = 123;

  MockRendererPpapiHost host(view_, pp_instance());
  PepperFileChooserHost chooser(&host, pp_instance(), pp_resource);

  // Say there's a user gesture.
  host.set_has_user_gesture(true);

  std::vector<std::string> accept;
  accept.push_back("text/plain");
  PpapiHostMsg_FileChooser_Show show_msg(false, false, std::string(), accept);

  ppapi::proxy::ResourceMessageCallParams call_params(pp_resource, 0);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = chooser.OnResourceMessageReceived(show_msg, &context);
  EXPECT_EQ(PP_OK_COMPLETIONPENDING, result);

  // The render view should have sent a chooser request to the browser
  // (caught by the render thread's test message sink).
  const IPC::Message* msg = render_thread_->sink().GetUniqueMessageMatching(
      FrameHostMsg_RunFileChooser::ID);
  ASSERT_TRUE(msg);
  FrameHostMsg_RunFileChooser::Schema::Param call_msg_param;
  ASSERT_TRUE(FrameHostMsg_RunFileChooser::Read(msg, &call_msg_param));
  const blink::mojom::FileChooserParams& chooser_params =
      std::get<0>(call_msg_param);

  // Basic validation of request.
  EXPECT_EQ(blink::mojom::FileChooserParams::Mode::kOpen, chooser_params.mode);
  ASSERT_EQ(1u, chooser_params.accept_types.size());
  EXPECT_EQ(accept[0], base::UTF16ToUTF8(chooser_params.accept_types[0]));

  // Send a chooser reply to the render view. Note our reply path has to have a
  // path separator so we include both a Unix and a Windows one.
  std::vector<blink::mojom::FileChooserFileInfoPtr> selected_info_vector;
  selected_info_vector.push_back(
      blink::mojom::FileChooserFileInfo::NewNativeFile(
          blink::mojom::NativeFileInfo::New(
              base::FilePath(FILE_PATH_LITERAL("myp\\ath/foo")),
              base::ASCIIToUTF16("Hello, world"))));
  RenderFrameImpl* frame_impl =
      static_cast<RenderFrameImpl*>(view_->GetMainRenderFrame());
  FrameMsg_RunFileChooserResponse response(frame_impl->GetRoutingID(),
                                           selected_info_vector);
  EXPECT_TRUE(frame_impl->OnMessageReceived(response));

  // This should have sent the Pepper reply to our test sink.
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(host.sink().GetFirstResourceReplyMatching(
      PpapiPluginMsg_FileChooser_ShowReply::ID, &reply_params, &reply_msg));

  // Basic validation of reply.
  EXPECT_EQ(call_params.sequence(), reply_params.sequence());
  EXPECT_EQ(PP_OK, reply_params.result());
  PpapiPluginMsg_FileChooser_ShowReply::Schema::Param reply_msg_param;
  ASSERT_TRUE(
      PpapiPluginMsg_FileChooser_ShowReply::Read(&reply_msg, &reply_msg_param));
  const std::vector<ppapi::FileRefCreateInfo>& chooser_results =
      std::get<0>(reply_msg_param);
  ASSERT_EQ(1u, chooser_results.size());
  EXPECT_EQ(base::UTF16ToUTF8(
                selected_info_vector[0]->get_native_file()->display_name),
            chooser_results[0].display_name);
}

TEST_F(PepperFileChooserHostTest, NoUserGesture) {
  PP_Resource pp_resource = 123;

  MockRendererPpapiHost host(view_, pp_instance());
  PepperFileChooserHost chooser(&host, pp_instance(), pp_resource);

  // Say there's no user gesture.
  host.set_has_user_gesture(false);

  std::vector<std::string> accept;
  accept.push_back("text/plain");
  PpapiHostMsg_FileChooser_Show show_msg(false, false, std::string(), accept);

  ppapi::proxy::ResourceMessageCallParams call_params(pp_resource, 0);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = chooser.OnResourceMessageReceived(show_msg, &context);
  EXPECT_EQ(PP_ERROR_NO_USER_GESTURE, result);
}

}  // namespace content
