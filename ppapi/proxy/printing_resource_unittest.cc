// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstring>

#include "ppapi/c/dev/ppb_printing_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/locking_resource_releaser.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/printing_resource.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest PrintingResourceTest;

bool g_callback_called;
int32_t g_callback_result;

void Callback(void* user_data, int32_t result) {
  g_callback_called = true;
  g_callback_result = result;
}

bool PP_SizeEqual(const PP_Size& lhs, const PP_Size& rhs) {
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

bool PP_RectEqual(const PP_Rect& lhs, const PP_Rect& rhs) {
    return lhs.point.x == rhs.point.x &&
        lhs.point.y == rhs.point.y &&
        PP_SizeEqual(lhs.size, rhs.size);
}

}  // namespace

// Does a full test of GetDefaultPrintSettings() and reply functionality in the
// plugin side using the public C interfaces.
TEST_F(PrintingResourceTest, GetDefaultPrintSettings) {
  g_callback_called = false;

  const PPB_Printing_Dev_0_7* printing_iface =
      thunk::GetPPB_Printing_Dev_0_7_Thunk();
  LockingResourceReleaser res(printing_iface->Create(pp_instance()));

  PP_PrintSettings_Dev output_settings;

  int32_t result = printing_iface->GetDefaultPrintSettings(
      res.get(), &output_settings, PP_MakeCompletionCallback(&Callback, NULL));
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  // Should have sent a "GetDefaultPrintSettings" message.
  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_Printing_GetDefaultPrintSettings::ID, &params, &msg));

  // Synthesize a response with some random print settings.
  ResourceMessageReplyParams reply_params(params.pp_resource(),
                                          params.sequence());
  reply_params.set_result(PP_OK);

  PP_PrintSettings_Dev reply_settings = {{{0, 0}, {500, 515}},
                                         {{25, 35}, {300, 720}},
                                         {600, 700},
                                         200,
                                         PP_PRINTORIENTATION_NORMAL,
                                         PP_PRINTSCALINGOPTION_NONE,
                                         PP_FALSE,
                                         PP_PRINTOUTPUTFORMAT_PDF};
  PluginMessageFilter::DispatchResourceReplyForTest(
      reply_params,
      PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply(
          reply_settings));

  EXPECT_TRUE(PP_RectEqual(reply_settings.printable_area,
                           output_settings.printable_area));
  EXPECT_TRUE(PP_RectEqual(reply_settings.content_area,
                           output_settings.content_area));
  EXPECT_TRUE(PP_SizeEqual(reply_settings.paper_size,
                           output_settings.paper_size));
  EXPECT_EQ(reply_settings.dpi, output_settings.dpi);
  EXPECT_EQ(reply_settings.orientation, output_settings.orientation);
  EXPECT_EQ(reply_settings.print_scaling_option,
            output_settings.print_scaling_option);
  EXPECT_EQ(reply_settings.grayscale, output_settings.grayscale);
  EXPECT_EQ(reply_settings.format, output_settings.format);

  EXPECT_EQ(g_callback_result, PP_OK);
  EXPECT_EQ(g_callback_called, true);
}

}  // namespace proxy
}  // namespace ppapi
