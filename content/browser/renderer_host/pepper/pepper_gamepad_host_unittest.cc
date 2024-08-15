// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/pepper/pepper_gamepad_host.h"

#include <stddef.h>
#include <string.h>

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/pepper/browser_ppapi_host_test.h"
#include "device/gamepad/gamepad_shared_buffer.h"
#include "device/gamepad/gamepad_test_helpers.h"
#include "device/gamepad/public/cpp/gamepads.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/proxy/gamepad_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

class PepperGamepadHostTest : public testing::Test,
                              public BrowserPpapiHostTest {
 public:
  PepperGamepadHostTest() {}

  PepperGamepadHostTest(const PepperGamepadHostTest&) = delete;
  PepperGamepadHostTest& operator=(const PepperGamepadHostTest&) = delete;

  ~PepperGamepadHostTest() override {}

  void ConstructService(const device::Gamepads& test_data) {
    service_ =
        std::make_unique<device::GamepadServiceTestConstructor>(test_data);
  }

  device::GamepadService* gamepad_service() {
    return service_->gamepad_service();
  }

 protected:
  std::unique_ptr<device::GamepadServiceTestConstructor> service_;
};

}  // namespace

TEST_F(PepperGamepadHostTest, WaitForReply) {
  device::Gamepads default_data;
  memset(&default_data, 0, sizeof(device::Gamepads));
  default_data.items[0].connected = true;
  default_data.items[0].buttons_length = 1;
  ConstructService(default_data);

  PP_Instance pp_instance = 12345;
  PP_Resource pp_resource = 67890;
  PepperGamepadHost gamepad_host(
      gamepad_service(), GetBrowserPpapiHost(), pp_instance, pp_resource);

  // Synthesize a request for gamepad data.
  ppapi::host::HostMessageContext context(
      ppapi::proxy::ResourceMessageCallParams(pp_resource, 1));
  EXPECT_EQ(PP_OK_COMPLETIONPENDING,
            gamepad_host.OnResourceMessageReceived(
                PpapiHostMsg_Gamepad_RequestMemory(), &context));

  device::MockGamepadDataFetcher* fetcher = service_->data_fetcher();
  fetcher->WaitForDataReadAndCallbacksIssued();

  // It should not have sent the callback message.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(0u, sink().message_count());

  // Set a button down and wait for it to be read twice.
  device::Gamepads button_down_data = default_data;
  button_down_data.items[0].buttons[0].value = 1.f;
  button_down_data.items[0].buttons[0].pressed = true;
  fetcher->SetTestData(button_down_data);
  fetcher->WaitForDataReadAndCallbacksIssued();

  // It should have sent a callback.
  base::RunLoop().RunUntilIdle();
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(sink().GetFirstResourceReplyMatching(
      PpapiPluginMsg_Gamepad_SendMemory::ID, &reply_params, &reply_msg));

  // Extract the shared memory region.
  base::ReadOnlySharedMemoryRegion shared_memory_region;
  EXPECT_TRUE(reply_params.TakeReadOnlySharedMemoryRegionAtIndex(
      0, &shared_memory_region));

  // Validate the shared memory.
  base::ReadOnlySharedMemoryMapping shared_memory_mapping =
      shared_memory_region.Map();
  EXPECT_TRUE(shared_memory_mapping.IsValid());
  const device::GamepadHardwareBuffer* buffer =
      static_cast<const device::GamepadHardwareBuffer*>(
          shared_memory_mapping.memory());
  EXPECT_EQ(button_down_data.items[0].buttons_length,
            buffer->data.items[0].buttons_length);
  for (size_t i = 0; i < device::Gamepad::kButtonsLengthCap; i++) {
    // Gamepad data is packed, so `value` is misaligned. `EXPECT_EQ` internally
    // takes a reference to the value, so we must copy into a correctly-aligned
    // temporary first.
    // TODO(crbug.com/342213636): Rewrite to remove the need for UNSAFE_BUFFERS
    // annotation.
    UNSAFE_TODO(EXPECT_EQ(double{button_down_data.items[0].buttons[i].value},
                          double{buffer->data.items[0].buttons[i].value});
                EXPECT_EQ(button_down_data.items[0].buttons[i].pressed,
                          buffer->data.items[0].buttons[i].pressed););
  }

  // Duplicate requests should be denied.
  EXPECT_EQ(PP_ERROR_FAILED,
            gamepad_host.OnResourceMessageReceived(
                PpapiHostMsg_Gamepad_RequestMemory(), &context));
}

}  // namespace content
