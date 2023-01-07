// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <tuple>

#include "ipc/ipc_test_sink.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/proxy/plugin_var_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/proxy_object_var.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

PP_Var MakeObject(int32_t object_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object_id;
  return ret;
}

// A Deallocate() function for PPP_Class that just increments the integer
// referenced by the pointer so we know how often Deallocate was called.
void MarkOnDeallocate(void* object) {
  (*static_cast<int*>(object))++;
}

// A class that just implements MarkOnDeallocate on destruction.
PPP_Class_Deprecated mark_on_deallocate_class = {
  NULL,  // HasProperty,
  NULL,  // HasMethod,
  NULL,  // GetProperty,
  NULL,  // GetAllPropertyNames,
  NULL,  // SetProperty,
  NULL,  // RemoveProperty,
  NULL,  // Call,
  NULL,  // Construct,
  &MarkOnDeallocate
};

}  // namespace

class PluginVarTrackerTest : public PluginProxyTest {
 public:
  PluginVarTrackerTest() {}

 protected:
  // Asserts that there is a unique "release object" IPC message in the test
  // sink. This will return the var ID from the message or -1 if none found.
  int32_t GetObjectIDForUniqueReleaseObject() {
    const IPC::Message* release_msg = sink().GetUniqueMessageMatching(
        PpapiHostMsg_PPBVar_ReleaseObject::ID);
    if (!release_msg)
      return -1;

    std::tuple<int64_t> id;
    PpapiHostMsg_PPBVar_ReleaseObject::Read(release_msg, &id);
    return std::get<0>(id);
  }
};

TEST_F(PluginVarTrackerTest, GetHostObject) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObject(12345);

  // Round-trip through the tracker to make sure the host object comes out the
  // other end.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  PP_Var host_object2 = var_tracker().GetHostObject(plugin_object);
  EXPECT_EQ(PP_VARTYPE_OBJECT, host_object2.type);
  EXPECT_EQ(host_object.value.as_id, host_object2.value.as_id);

  var_tracker().ReleaseVar(plugin_object);
}

TEST_F(PluginVarTrackerTest, ReceiveObjectPassRef) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObject(12345);

  // Receive the object, we should have one ref and no messages.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  EXPECT_EQ(0u, sink().message_count());
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(0,
      var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_object));

  // Receive the same object again, we should get the same plugin ID out.
  PP_Var plugin_object2 = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  EXPECT_EQ(plugin_object.value.as_id, plugin_object2.value.as_id);
  EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(0,
      var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_object));

  // It should have sent one message to decerment the refcount in the host.
  // This is because it only maintains one host refcount for all references
  // in the plugin, but the host just sent the second one.
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());
  sink().ClearMessages();

  // Release the object, one ref at a time. The second release should free
  // the tracking data and send a release message to the browser.
  var_tracker().ReleaseVar(plugin_object);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  var_tracker().ReleaseVar(plugin_object);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());
}

// Tests freeing objects that have both refcounts and "tracked with no ref".
TEST_F(PluginVarTrackerTest, FreeTrackedAndReferencedObject) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObject(12345);

  // Phase one: First receive via a "pass ref", then a tracked with no ref.
  PP_Var plugin_var = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  PP_Var plugin_var2 = var_tracker().TrackObjectWithNoReference(
      host_object, plugin_dispatcher());
  EXPECT_EQ(plugin_var.value.as_id, plugin_var2.value.as_id);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));

  // Free via the refcount, this should release the object to the browser but
  // maintain the tracked object.
  var_tracker().ReleaseVar(plugin_var);
  EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(1u, sink().message_count());
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());

  // Now free via the tracked object, this should free it.
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_var));

  // Phase two: Receive via a tracked, then get an addref.
  sink().ClearMessages();
  plugin_var = var_tracker().TrackObjectWithNoReference(
      host_object, plugin_dispatcher());
  plugin_var2 = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  EXPECT_EQ(plugin_var.value.as_id, plugin_var2.value.as_id);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));

  // Free via the tracked object, this should have no effect.
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(0,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
  EXPECT_EQ(0u, sink().message_count());

  // Now free via the refcount, this should delete it.
  var_tracker().ReleaseVar(plugin_var);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(host_object.value.as_id, GetObjectIDForUniqueReleaseObject());
}

TEST_F(PluginVarTrackerTest, RecursiveTrackWithNoRef) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObject(12345);

  // Receive a tracked object twice.
  PP_Var plugin_var = var_tracker().TrackObjectWithNoReference(
      host_object, plugin_dispatcher());
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
  PP_Var plugin_var2 = var_tracker().TrackObjectWithNoReference(
      host_object, plugin_dispatcher());
  EXPECT_EQ(plugin_var.value.as_id, plugin_var2.value.as_id);
  EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_var));
  EXPECT_EQ(2,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));

  // Now release those tracked items, the reference should be freed.
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
  var_tracker().StopTrackingObjectWithNoReference(plugin_var);
  EXPECT_EQ(-1,
            var_tracker().GetTrackedWithNoReferenceCountForObject(plugin_var));
}

// Tests that objects implemented by the plugin that have no references by
// the plugin get their Deallocate function called on destruction.
TEST_F(PluginVarTrackerTest, PluginObjectInstanceDeleted) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObject(12345);
  PP_Instance pp_instance = 0x12345;

  int deallocate_called = 0;
  void* user_data = &deallocate_called;

  // Make a var with one reference.
  scoped_refptr<ProxyObjectVar> object(
      new ProxyObjectVar(plugin_dispatcher(), host_object.value.as_id));
  PP_Var plugin_var = MakeObject(var_tracker().AddVar(object.get()));
  var_tracker().PluginImplementedObjectCreated(
      pp_instance, plugin_var, &mark_on_deallocate_class, user_data);

  // Release the plugin ref to the var. WebKit hasn't called destroy so
  // we won't get a destroy call.
  object.reset();
  var_tracker().ReleaseVar(plugin_var);
  EXPECT_EQ(0, deallocate_called);

  // Synthesize an instance destuction, this should call Deallocate.
  var_tracker().DidDeleteInstance(pp_instance);
  EXPECT_EQ(1, deallocate_called);
}

// Tests what happens when a plugin keeps a ref to a plugin-implemented
// object var longer than the instance. We should not call the destructor until
// the plugin releases its last ref.
TEST_F(PluginVarTrackerTest, PluginObjectLeaked) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObject(12345);
  PP_Instance pp_instance = 0x12345;

  int deallocate_called = 0;
  void* user_data = &deallocate_called;

  // Make a var with one reference.
  scoped_refptr<ProxyObjectVar> object(
      new ProxyObjectVar(plugin_dispatcher(), host_object.value.as_id));
  PP_Var plugin_var = MakeObject(var_tracker().AddVar(object.get()));
  var_tracker().PluginImplementedObjectCreated(
      pp_instance, plugin_var, &mark_on_deallocate_class, user_data);

  // Destroy the instance. This should not call deallocate since the plugin
  // still has a ref.
  var_tracker().DidDeleteInstance(pp_instance);
  EXPECT_EQ(0, deallocate_called);

  // Release the plugin ref to the var. Since the instance is gone this should
  // call deallocate.
  object.reset();
  var_tracker().ReleaseVar(plugin_var);
  EXPECT_EQ(1, deallocate_called);
}

}  // namespace proxy
}  // namespace ppapi
