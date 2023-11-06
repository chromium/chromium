// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <map>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "content/renderer/pepper/pepper_device_enumeration_host_helper.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/host/host_message_context.h"
#include "ppapi/host/ppapi_host.h"
#include "ppapi/host/resource_host.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/resource_message_params.h"
#include "ppapi/proxy/resource_message_test_sink.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace content {

namespace {

std::vector<ppapi::DeviceRefData> TestEnumerationData() {
  std::vector<ppapi::DeviceRefData> data;
  ppapi::DeviceRefData data_item;
  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_1";
  data_item.id = "id_1";
  data.push_back(data_item);
  data_item.type = PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  data_item.name = "name_2";
  data_item.id = "id_2";
  data.push_back(data_item);

  return data;
}

class TestDelegate : public PepperDeviceEnumerationHostHelper::Delegate {
 public:
  TestDelegate() : last_used_id_(0u) {}

  TestDelegate(const TestDelegate&) = delete;
  TestDelegate& operator=(const TestDelegate&) = delete;

  ~TestDelegate() override { CHECK(monitoring_callbacks_.empty()); }

  void EnumerateDevices(PP_DeviceType_Dev /* type */,
                        DevicesOnceCallback callback) override {
    std::move(callback).Run(TestEnumerationData());
  }

  size_t StartMonitoringDevices(PP_DeviceType_Dev /* type */,
                                const DevicesCallback& callback) override {
    last_used_id_++;
    monitoring_callbacks_[last_used_id_] = callback;
    return last_used_id_;
  }

  void StopMonitoringDevices(PP_DeviceType_Dev /* type */,
                             size_t subscription_id) override {
    auto iter = monitoring_callbacks_.find(subscription_id);
    CHECK(iter != monitoring_callbacks_.end());
    monitoring_callbacks_.erase(iter);
  }

  // Returns false if |request_id| is not found.
  bool SimulateDevicesChanged(
      size_t subscription_id,
      const std::vector<ppapi::DeviceRefData>& devices) {
    auto iter = monitoring_callbacks_.find(subscription_id);
    if (iter == monitoring_callbacks_.end())
      return false;

    iter->second.Run(devices);
    return true;
  }

  size_t GetRegisteredCallbackCount() const {
    return monitoring_callbacks_.size();
  }

  size_t last_used_id() const { return last_used_id_; }

  base::WeakPtr<TestDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  std::map<size_t, DevicesCallback> monitoring_callbacks_;
  size_t last_used_id_;
  base::WeakPtrFactory<TestDelegate> weak_ptr_factory_{this};
};

class PepperDeviceEnumerationHostHelperTest : public testing::Test {
 public:
  PepperDeviceEnumerationHostHelperTest(
      const PepperDeviceEnumerationHostHelperTest&) = delete;
  PepperDeviceEnumerationHostHelperTest& operator=(
      const PepperDeviceEnumerationHostHelperTest&) = delete;

 protected:
  PepperDeviceEnumerationHostHelperTest()
      : ppapi_host_(&sink_, ppapi::PpapiPermissions()),
        resource_host_(&ppapi_host_, 12345, 67890),
        device_enumeration_(&resource_host_,
                            delegate_.AsWeakPtr(),
                            PP_DEVICETYPE_DEV_AUDIOCAPTURE,
                            GURL("http://example.com")) {}

  ~PepperDeviceEnumerationHostHelperTest() override {}

  void SimulateMonitorDeviceChangeReceived(uint32_t callback_id) {
    PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange msg(callback_id);
    ppapi::proxy::ResourceMessageCallParams call_params(
        resource_host_.pp_resource(), 123);
    ppapi::host::HostMessageContext context(call_params);
    int32_t result = PP_ERROR_FAILED;
    ASSERT_TRUE(
        device_enumeration_.HandleResourceMessage(msg, &context, &result));
    EXPECT_EQ(PP_OK, result);
  }

  void CheckNotifyDeviceChangeMessage(
      uint32_t callback_id,
      const std::vector<ppapi::DeviceRefData>& expected) {
    ppapi::proxy::ResourceMessageReplyParams reply_params;
    IPC::Message reply_msg;
    ASSERT_TRUE(sink_.GetFirstResourceReplyMatching(
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange::ID,
        &reply_params,
        &reply_msg));
    sink_.ClearMessages();

    EXPECT_EQ(PP_OK, reply_params.result());

    uint32_t reply_callback_id = 0;
    std::vector<ppapi::DeviceRefData> reply_data;
    ASSERT_TRUE(ppapi::UnpackMessage<
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange>(
        reply_msg, &reply_callback_id, &reply_data));
    EXPECT_EQ(callback_id, reply_callback_id);
    EXPECT_EQ(expected, reply_data);
  }

  TestDelegate delegate_;
  ppapi::proxy::ResourceMessageTestSink sink_;
  ppapi::host::PpapiHost ppapi_host_;
  ppapi::host::ResourceHost resource_host_;
  PepperDeviceEnumerationHostHelper device_enumeration_;
  base::test::SingleThreadTaskEnvironment
      task_environment_;  // required for async calls to work.
};

}  // namespace

TEST_F(PepperDeviceEnumerationHostHelperTest, EnumerateDevices) {
  PpapiHostMsg_DeviceEnumeration_EnumerateDevices msg;
  ppapi::proxy::ResourceMessageCallParams call_params(
      resource_host_.pp_resource(), 123);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = PP_ERROR_FAILED;
  ASSERT_TRUE(
      device_enumeration_.HandleResourceMessage(msg, &context, &result));
  EXPECT_EQ(PP_OK_COMPLETIONPENDING, result);
  base::RunLoop().RunUntilIdle();

  // A reply message should have been sent to the test sink.
  ppapi::proxy::ResourceMessageReplyParams reply_params;
  IPC::Message reply_msg;
  ASSERT_TRUE(sink_.GetFirstResourceReplyMatching(
      PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply::ID,
      &reply_params,
      &reply_msg));

  EXPECT_EQ(call_params.sequence(), reply_params.sequence());
  EXPECT_EQ(PP_OK, reply_params.result());

  std::vector<ppapi::DeviceRefData> reply_data;
  ASSERT_TRUE(ppapi::UnpackMessage<
      PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply>(reply_msg,
                                                              &reply_data));
  EXPECT_EQ(TestEnumerationData(), reply_data);
}

TEST_F(PepperDeviceEnumerationHostHelperTest, MonitorDeviceChange) {
  uint32_t callback_id = 456;
  SimulateMonitorDeviceChangeReceived(callback_id);

  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());
  size_t request_id = delegate_.last_used_id();

  std::vector<ppapi::DeviceRefData> data;
  ASSERT_TRUE(delegate_.SimulateDevicesChanged(request_id, data));

  // StopEnumerateDevices() shouldn't be called because the MonitorDeviceChange
  // message is a persistent request.
  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());

  CheckNotifyDeviceChangeMessage(callback_id, data);

  ppapi::DeviceRefData data_item;
  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_1";
  data_item.id = "id_1";
  data.push_back(data_item);
  data_item.type = PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  data_item.name = "name_2";
  data_item.id = "id_2";
  data.push_back(data_item);
  ASSERT_TRUE(delegate_.SimulateDevicesChanged(request_id, data));
  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());

  CheckNotifyDeviceChangeMessage(callback_id, data);

  uint32_t callback_id2 = 789;
  SimulateMonitorDeviceChangeReceived(callback_id2);

  // StopEnumerateDevice() should have been called for the previous request.
  EXPECT_EQ(1U, delegate_.GetRegisteredCallbackCount());
  size_t request_id2 = delegate_.last_used_id();

  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_3";
  data_item.id = "id_3";
  data.push_back(data_item);
  ASSERT_TRUE(delegate_.SimulateDevicesChanged(request_id2, data));

  CheckNotifyDeviceChangeMessage(callback_id2, data);

  PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange msg;
  ppapi::proxy::ResourceMessageCallParams call_params(
      resource_host_.pp_resource(), 123);
  ppapi::host::HostMessageContext context(call_params);
  int32_t result = PP_ERROR_FAILED;
  ASSERT_TRUE(
      device_enumeration_.HandleResourceMessage(msg, &context, &result));
  EXPECT_EQ(PP_OK, result);

  EXPECT_EQ(0U, delegate_.GetRegisteredCallbackCount());
}

}  // namespace content
