// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_printing_host.h"

#include <stdint.h>
#include <tuple>
#include <utility>

#include "content/browser/renderer_host/pepper/browser_ppapi_host_test.h"
#include "content/browser/renderer_host/pepper/pepper_print_settings_manager.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_message_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// Mock implementation of |PepperPrintSettingsManager| for test purposes.
class MockPepperPrintSettingsManager : public PepperPrintSettingsManager {
 public:
  MockPepperPrintSettingsManager(const PP_PrintSettings_Dev& settings);

  MockPepperPrintSettingsManager(const MockPepperPrintSettingsManager&) =
      delete;
  MockPepperPrintSettingsManager& operator=(
      const MockPepperPrintSettingsManager&) = delete;

  ~MockPepperPrintSettingsManager() override {}

  // PepperPrintSettingsManager implementation.
  void GetDefaultPrintSettings(
      PepperPrintSettingsManager::Callback callback) override;

 private:
  PP_PrintSettings_Dev settings_;
};

MockPepperPrintSettingsManager::MockPepperPrintSettingsManager(
    const PP_PrintSettings_Dev& settings)
    : settings_(settings) {}

void MockPepperPrintSettingsManager::GetDefaultPrintSettings(
    PepperPrintSettingsManager::Callback callback) {
  std::move(callback).Run(PepperPrintSettingsManager::Result(settings_, PP_OK));
}

class PepperPrintingHostTest : public testing::Test,
                               public BrowserPpapiHostTest {
 public:
  PepperPrintingHostTest() {}

  PepperPrintingHostTest(const PepperPrintingHostTest&) = delete;
  PepperPrintingHostTest& operator=(const PepperPrintingHostTest&) = delete;

  ~PepperPrintingHostTest() override {}
};

bool PP_SizeEqual(const PP_Size& lhs, const PP_Size& rhs) {
  return lhs.width == rhs.width && lhs.height == rhs.height;
}

bool PP_RectEqual(const PP_Rect& lhs, const PP_Rect& rhs) {
  return lhs.point.x == rhs.point.x && lhs.point.y == rhs.point.y &&
         PP_SizeEqual(lhs.size, rhs.size);
}

}  // namespace

TEST_F(PepperPrintingHostTest, GetDefaultPrintSettings) {
  PP_Instance pp_instance = 12345;
  PP_Resource pp_resource = 67890;
  PP_PrintSettings_Dev expected_settings = {{{0, 0}, {500, 515}},
                                            {{25, 35}, {300, 720}},
                                            {600, 700},
                                            200,
                                            PP_PRINTORIENTATION_NORMAL,
                                            PP_PRINTSCALINGOPTION_NONE,
                                            PP_FALSE,
                                            PP_PRINTOUTPUTFORMAT_PDF};

  // Construct the resource host.
  std::unique_ptr<PepperPrintSettingsManager> manager(
      new MockPepperPrintSettingsManager(expected_settings));
  PepperPrintingHost printing(GetBrowserPpapiHost()->GetPpapiHost(),
                              pp_instance, pp_resource, std::move(manager));

  // Simulate a message being received.
  ppapi::proxy::ResourceMessageCallParams call_params(pp_resource, 1);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = printing.OnResourceMessageReceived(
      PpapiHostMsg_Printing_GetDefaultPrintSettings(), &context);
  EXPECT_EQ(PP_OK_COMPLETIONPENDING, result);

  // This should have sent the Pepper reply to our test sink.
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(sink().GetFirstResourceReplyMatching(
      PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply::ID,
      &reply_params,
      &reply_msg));

  // Validation of reply.
  EXPECT_EQ(call_params.sequence(), reply_params.sequence());
  EXPECT_EQ(PP_OK, reply_params.result());
  PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply::Schema::Param
      reply_msg_param;
  ASSERT_TRUE(PpapiPluginMsg_Printing_GetDefaultPrintSettingsReply::Read(
      &reply_msg, &reply_msg_param));
  PP_PrintSettings_Dev actual_settings = std::get<0>(reply_msg_param);

  EXPECT_TRUE(PP_RectEqual(expected_settings.printable_area,
                           actual_settings.printable_area));
  EXPECT_TRUE(PP_RectEqual(expected_settings.content_area,
                           actual_settings.content_area));
  EXPECT_TRUE(
      PP_SizeEqual(expected_settings.paper_size, actual_settings.paper_size));
  EXPECT_EQ(expected_settings.dpi, actual_settings.dpi);
  EXPECT_EQ(expected_settings.orientation, actual_settings.orientation);
  EXPECT_EQ(expected_settings.print_scaling_option,
            actual_settings.print_scaling_option);
  EXPECT_EQ(expected_settings.grayscale, actual_settings.grayscale);
  EXPECT_EQ(expected_settings.format, actual_settings.format);
}

}  // namespace content
