// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/connection.h"
#include "ppapi/proxy/device_enumeration_resource_helper.h"
#include "ppapi/proxy/plugin_message_filter.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_message_utils.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/ppb_device_ref_shared.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_device_ref_api.h"
#include "ppapi/thunk/thunk.h"

namespace ppapi {
namespace proxy {

namespace {

typedef PluginProxyTest DeviceEnumerationResourceHelperTest;

Connection GetConnection(PluginProxyTestHarness* harness) {
  CHECK(harness->GetGlobals()->IsPluginGlobals());

  return Connection(
      static_cast<PluginGlobals*>(harness->GetGlobals())->GetBrowserSender(),
      harness->plugin_dispatcher(), 0);
}

bool CompareDeviceRef(PluginVarTracker* var_tracker,
                      PP_Resource resource,
                      const DeviceRefData& expected) {
  thunk::EnterResourceNoLock<thunk::PPB_DeviceRef_API> enter(resource, true);
  if (enter.failed())
    return false;

  if (expected.type != enter.object()->GetType())
    return false;

  PP_Var name_pp_var = enter.object()->GetName();
  bool result = false;
  do {
    Var* name_var = var_tracker->GetVar(name_pp_var);
    if (!name_var)
      break;
    StringVar* name_string_var = name_var->AsStringVar();
    if (!name_string_var)
      break;
    if (expected.name != name_string_var->value())
      break;

    result = true;
  } while (false);
  var_tracker->ReleaseVar(name_pp_var);
  return result;
}

class TestResource : public PluginResource {
 public:
  TestResource(Connection connection, PP_Instance instance)
      : PluginResource(connection, instance),
        device_enumeration_(this) {
  }

  TestResource(const TestResource&) = delete;
  TestResource& operator=(const TestResource&) = delete;

  ~TestResource() override {}

  void OnReplyReceived(const ResourceMessageReplyParams& params,
                       const IPC::Message& msg) override {
    if (!device_enumeration_.HandleReply(params, msg))
      PluginResource::OnReplyReceived(params, msg);
  }

  DeviceEnumerationResourceHelper& device_enumeration() {
    return device_enumeration_;
  }

 private:
  DeviceEnumerationResourceHelper device_enumeration_;
};

class TestCallback {
 public:
  TestCallback() : called_(false), result_(PP_ERROR_FAILED) {
  }

  TestCallback(const TestCallback&) = delete;
  TestCallback& operator=(const TestCallback&) = delete;

  ~TestCallback() {
    CHECK(called_);
  }

  PP_CompletionCallback MakeCompletionCallback() {
    return PP_MakeCompletionCallback(&CompletionCallbackBody, this);
  }

  bool called() const { return called_; }
  int32_t result() const { return result_; }

 private:
  static void CompletionCallbackBody(void* user_data, int32_t result) {
    TestCallback* callback = static_cast<TestCallback*>(user_data);

    CHECK(!callback->called_);
    callback->called_ = true;
    callback->result_ = result;
  }

  bool called_;
  int32_t result_;
};

class TestArrayOutput {
 public:
  explicit TestArrayOutput(PluginResourceTracker* resource_tracker)
      : data_(NULL),
        count_(0),
        resource_tracker_(resource_tracker) {
  }

  TestArrayOutput(const TestArrayOutput&) = delete;
  TestArrayOutput& operator=(const TestArrayOutput&) = delete;

  ~TestArrayOutput() {
    if (count_ > 0) {
      for (size_t i = 0; i < count_; ++i)
        resource_tracker_->ReleaseResource(data_[i]);
      delete [] data_;
    }
  }

  PP_ArrayOutput MakeArrayOutput() {
    PP_ArrayOutput array_output = { &GetDataBuffer, this };
    return array_output;
  }

  const PP_Resource* data() const { return data_; }
  uint32_t count() const { return count_; }

 private:
  static void* GetDataBuffer(void* user_data,
                             uint32_t element_count,
                             uint32_t element_size) {
    CHECK_EQ(element_size, sizeof(PP_Resource));

    TestArrayOutput* output = static_cast<TestArrayOutput*>(user_data);
    CHECK(!output->data_);

    output->count_ = element_count;
    if (element_count > 0)
      output->data_ = new PP_Resource[element_count];
    else
      output->data_ = NULL;

    return output->data_;
  }

  PP_Resource* data_;
  uint32_t count_;
  PluginResourceTracker* resource_tracker_;
};

class TestMonitorDeviceChange {
 public:
  explicit TestMonitorDeviceChange(PluginVarTracker* var_tracker)
      : called_(false),
        same_as_expected_(false),
        var_tracker_(var_tracker) {
  }

  TestMonitorDeviceChange(const TestMonitorDeviceChange&) = delete;
  TestMonitorDeviceChange& operator=(const TestMonitorDeviceChange&) = delete;

  ~TestMonitorDeviceChange() {}

  void SetExpectedResult(const std::vector<DeviceRefData>& expected) {
    called_ = false;
    same_as_expected_ = false;
    expected_ = expected;
  }

  bool called() const { return called_; }

  bool same_as_expected() const { return same_as_expected_; }

  static void MonitorDeviceChangeCallback(void* user_data,
                                          uint32_t device_count,
                                          const PP_Resource devices[]) {
    ProxyAutoLock lock;
    TestMonitorDeviceChange* helper =
        static_cast<TestMonitorDeviceChange*>(user_data);
    CHECK(!helper->called_);

    helper->called_ = true;
    helper->same_as_expected_ = false;
    if (device_count != helper->expected_.size())
      return;
    for (size_t i = 0; i < device_count; ++i) {
      if (!CompareDeviceRef(helper->var_tracker_, devices[i],
                            helper->expected_[i])) {
        return;
      }
    }
    helper->same_as_expected_ = true;
  }

 private:
  bool called_;
  bool same_as_expected_;
  std::vector<DeviceRefData> expected_;
  PluginVarTracker* var_tracker_;
};

}  // namespace

TEST_F(DeviceEnumerationResourceHelperTest, EnumerateDevices) {
  ProxyAutoLock lock;

  scoped_refptr<TestResource> resource(
      new TestResource(GetConnection(this), pp_instance()));
  DeviceEnumerationResourceHelper& device_enumeration =
      resource->device_enumeration();

  TestArrayOutput output(&resource_tracker());
  TestCallback callback;
  scoped_refptr<TrackedCallback> tracked_callback(
      new TrackedCallback(resource.get(), callback.MakeCompletionCallback()));
  int32_t result = device_enumeration.EnumerateDevices(output.MakeArrayOutput(),
                                                       tracked_callback);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);

  // Should have sent an EnumerateDevices message.
  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_DeviceEnumeration_EnumerateDevices::ID, &params, &msg));

  // Synthesize a response.
  ResourceMessageReplyParams reply_params(params.pp_resource(),
                                          params.sequence());
  reply_params.set_result(PP_OK);
  std::vector<DeviceRefData> data;
  DeviceRefData data_item;
  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_1";
  data_item.id = "id_1";
  data.push_back(data_item);
  data_item.type = PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  data_item.name = "name_2";
  data_item.id = "id_2";
  data.push_back(data_item);

  {
    ProxyAutoUnlock unlock;
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_DeviceEnumeration_EnumerateDevicesReply(data));
  }
  EXPECT_TRUE(callback.called());
  EXPECT_EQ(PP_OK, callback.result());
  EXPECT_EQ(2U, output.count());
  for (size_t i = 0; i < output.count(); ++i)
    EXPECT_TRUE(CompareDeviceRef(&var_tracker(), output.data()[i], data[i]));
}

TEST_F(DeviceEnumerationResourceHelperTest, MonitorDeviceChange) {
  ProxyAutoLock lock;

  scoped_refptr<TestResource> resource(
      new TestResource(GetConnection(this), pp_instance()));
  DeviceEnumerationResourceHelper& device_enumeration =
      resource->device_enumeration();

  TestMonitorDeviceChange helper(&var_tracker());

  int32_t result = device_enumeration.MonitorDeviceChange(
      &TestMonitorDeviceChange::MonitorDeviceChangeCallback, &helper);
  ASSERT_EQ(PP_OK, result);

  // Should have sent a MonitorDeviceChange message.
  ResourceMessageCallParams params;
  IPC::Message msg;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange::ID, &params, &msg));
  sink().ClearMessages();

  uint32_t callback_id = 0;
  ASSERT_TRUE(UnpackMessage<PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange>(
      msg, &callback_id));

  ResourceMessageReplyParams reply_params(params.pp_resource(), 0);
  reply_params.set_result(PP_OK);
  std::vector<DeviceRefData> data;

  helper.SetExpectedResult(data);

  {
    ProxyAutoUnlock unlock;
    // Synthesize a response with no device.
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(
            callback_id, data));
  }
  EXPECT_TRUE(helper.called() && helper.same_as_expected());

  DeviceRefData data_item;
  data_item.type = PP_DEVICETYPE_DEV_AUDIOCAPTURE;
  data_item.name = "name_1";
  data_item.id = "id_1";
  data.push_back(data_item);
  data_item.type = PP_DEVICETYPE_DEV_VIDEOCAPTURE;
  data_item.name = "name_2";
  data_item.id = "id_2";
  data.push_back(data_item);

  helper.SetExpectedResult(data);

  {
    ProxyAutoUnlock unlock;
    // Synthesize a response with some devices.
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(
            callback_id, data));
  }
  EXPECT_TRUE(helper.called() && helper.same_as_expected());

  TestMonitorDeviceChange helper2(&var_tracker());

  result = device_enumeration.MonitorDeviceChange(
      &TestMonitorDeviceChange::MonitorDeviceChangeCallback, &helper2);
  ASSERT_EQ(PP_OK, result);

  // Should have sent another MonitorDeviceChange message.
  ResourceMessageCallParams params2;
  IPC::Message msg2;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange::ID, &params2, &msg2));
  sink().ClearMessages();

  uint32_t callback_id2 = 0;
  ASSERT_TRUE(UnpackMessage<PpapiHostMsg_DeviceEnumeration_MonitorDeviceChange>(
      msg2, &callback_id2));

  helper.SetExpectedResult(data);
  helper2.SetExpectedResult(data);
  {
    ProxyAutoUnlock unlock;
    // |helper2| should receive the result while |helper| shouldn't.
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(
            callback_id2, data));
  }
  EXPECT_TRUE(helper2.called() && helper2.same_as_expected());
  EXPECT_FALSE(helper.called());

  helper.SetExpectedResult(data);
  helper2.SetExpectedResult(data);
  {
    ProxyAutoUnlock unlock;
    // Even if a message with |callback_id| arrives. |helper| shouldn't receive
    // the result.
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(
            callback_id, data));
  }
  EXPECT_FALSE(helper2.called());
  EXPECT_FALSE(helper.called());

  result = device_enumeration.MonitorDeviceChange(NULL, NULL);
  ASSERT_EQ(PP_OK, result);

  // Should have sent a StopMonitoringDeviceChange message.
  ResourceMessageCallParams params3;
  IPC::Message msg3;
  ASSERT_TRUE(sink().GetFirstResourceCallMatching(
      PpapiHostMsg_DeviceEnumeration_StopMonitoringDeviceChange::ID,
      &params3, &msg3));
  sink().ClearMessages();

  helper2.SetExpectedResult(data);
  {
    ProxyAutoUnlock unlock;
    // |helper2| shouldn't receive any result any more.
    PluginMessageFilter::DispatchResourceReplyForTest(
        reply_params,
        PpapiPluginMsg_DeviceEnumeration_NotifyDeviceChange(
            callback_id2, data));
  }
  EXPECT_FALSE(helper2.called());
}

}  // namespace proxy
}  // namespace ppapi
