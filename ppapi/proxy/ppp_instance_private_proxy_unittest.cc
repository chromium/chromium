// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include "base/functional/bind.h"
#include "base/test/test_timeouts.h"
#include "base/time/time.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/private/ppp_instance_private.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/interface_list.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/ppapi_permissions.h"
#include "ppapi/shared_impl/ppb_var_shared.h"
#include "ppapi/shared_impl/var.h"

namespace ppapi {

// A fake version of V8ObjectVar for testing.
class V8ObjectVar : public ppapi::Var {
 public:
  V8ObjectVar(const PPP_Class_Deprecated* ppp_class, void* ppp_class_data)
      : ppp_class_(ppp_class), ppp_class_data_(ppp_class_data) {}
  ~V8ObjectVar() override { ppp_class_->Deallocate(ppp_class_data_); }

  // Var overrides.
  V8ObjectVar* AsV8ObjectVar() override { return this; }
  PP_VarType GetType() const override { return PP_VARTYPE_OBJECT; }

 private:
  const PPP_Class_Deprecated* ppp_class_;
  void* ppp_class_data_;
};

namespace proxy {

namespace {
const PP_Instance kInstance = 0xdeadbeef;

PP_Var GetPPVarNoAddRef(Var* var) {
  PP_Var var_to_return = var->GetPPVar();
  PpapiGlobals::Get()->GetVarTracker()->ReleaseVar(var_to_return);
  return var_to_return;
}

PluginDispatcher* plugin_dispatcher = NULL;
// Return the plugin-side proxy for PPB_Var_Deprecated.
const PPB_Var_Deprecated* plugin_var_deprecated_if() {
  // The test code must set the plugin dispatcher.
  CHECK(plugin_dispatcher);
  // Grab the plugin-side proxy for PPB_Var_Deprecated (for CreateObject).
  return static_cast<const PPB_Var_Deprecated*>(
      plugin_dispatcher->GetBrowserInterface(
          PPB_VAR_DEPRECATED_INTERFACE));
}

// Mock PPP_Instance_Private.
PP_Var instance_obj;
PP_Var GetInstanceObject(PP_Instance /*instance*/) {
  // The 1 ref we got from CreateObject will be passed to the host. We want to
  // have a ref of our own.
  printf("GetInstanceObject called\n");
  plugin_var_deprecated_if()->AddRef(instance_obj);
  return instance_obj;
}

PPP_Instance_Private ppp_instance_private_mock = {
  &GetInstanceObject
};

// We need to pass in a |PPP_Class_Deprecated| to
// |PPB_Var_Deprecated->CreateObject| for a mock |Deallocate| method.
void Deallocate(void* object) {
}

const PPP_Class_Deprecated ppp_class_deprecated_mock = {
    NULL, // HasProperty
    NULL, // HasMethod
    NULL, // GetProperty
    NULL, // GetAllPropertyNames
    NULL, // SetProperty
    NULL, // RemoveProperty
    NULL, // Call
    NULL, // Construct
    &Deallocate
};


// We need to mock PPP_Instance, so that we can create and destroy the pretend
// instance that PPP_Instance_Private uses.
PP_Bool DidCreate(PP_Instance /*instance*/, uint32_t /*argc*/,
                  const char* /*argn*/[], const char* /*argv*/[]) {
  // Create an object var. This should exercise the typical path for creating
  // instance objects.
  instance_obj =
      plugin_var_deprecated_if()->CreateObject(kInstance,
                                               &ppp_class_deprecated_mock,
                                               NULL);
  return PP_TRUE;
}

void DidDestroy(PP_Instance /*instance*/) {
  // Decrement the reference count for our instance object. It should be
  // deleted.
  plugin_var_deprecated_if()->Release(instance_obj);
}

PPP_Instance_1_0 ppp_instance_mock = { &DidCreate, &DidDestroy };

// Mock PPB_Var_Deprecated, so that we can emulate creating an Object Var.
PP_Var CreateObject(PP_Instance /*instance*/,
                    const PPP_Class_Deprecated* ppp_class,
                    void* ppp_class_data) {
  V8ObjectVar* obj_var = new V8ObjectVar(ppp_class, ppp_class_data);
  return obj_var->GetPPVar();
}

const PPB_Var_Deprecated ppb_var_deprecated_mock = {
  PPB_Var_Shared::GetVarInterface1_0()->AddRef,
  PPB_Var_Shared::GetVarInterface1_0()->Release,
  PPB_Var_Shared::GetVarInterface1_0()->VarFromUtf8,
  PPB_Var_Shared::GetVarInterface1_0()->VarToUtf8,
  NULL, // HasProperty
  NULL, // HasMethod
  NULL, // GetProperty
  NULL, // EnumerateProperties
  NULL, // SetProperty
  NULL, // RemoveProperty
  NULL, // Call
  NULL, // Construct
  NULL, // IsInstanceOf
  &CreateObject
};

class PPP_Instance_Private_ProxyTest : public TwoWayTest {
 public:
   PPP_Instance_Private_ProxyTest()
       : TwoWayTest(TwoWayTest::TEST_PPP_INTERFACE) {
      plugin().RegisterTestInterface(PPP_INSTANCE_PRIVATE_INTERFACE,
                                     &ppp_instance_private_mock);
      plugin().RegisterTestInterface(PPP_INSTANCE_INTERFACE_1_0,
                                     &ppp_instance_mock);
      host().RegisterTestInterface(PPB_VAR_DEPRECATED_INTERFACE,
                                   &ppb_var_deprecated_mock);
  }
};

}  // namespace

TEST_F(PPP_Instance_Private_ProxyTest, PPPInstancePrivate) {
  // This test controls its own instance; we can't use the one that
  // PluginProxyTestHarness provides.
  ASSERT_NE(kInstance, pp_instance());
  HostDispatcher::SetForInstance(kInstance, host().host_dispatcher());

  // Requires dev interfaces.
  InterfaceList::SetProcessGlobalPermissions(
      PpapiPermissions::AllPermissions());

  // This file-local global is used by the PPP_Instance mock above in order to
  // access PPB_Var_Deprecated.
  plugin_dispatcher = plugin().plugin_dispatcher();

  // Grab the host-side proxy for PPP_Instance and PPP_Instance_Private.
  const PPP_Instance_Private* ppp_instance_private =
      static_cast<const PPP_Instance_Private*>(
          host().host_dispatcher()->GetProxiedInterface(
              PPP_INSTANCE_PRIVATE_INTERFACE));
  const PPP_Instance_1_1* ppp_instance = static_cast<const PPP_Instance_1_1*>(
      host().host_dispatcher()->GetProxiedInterface(
          PPP_INSTANCE_INTERFACE_1_1));

  // Initialize an Instance, so that the plugin-side machinery will work
  // properly.
  EXPECT_EQ(PP_TRUE, ppp_instance->DidCreate(kInstance, 0, NULL, NULL));

  // Check the plugin-side reference count.
  EXPECT_EQ(1, plugin().var_tracker().GetRefCountForObject(instance_obj));
  // Check the host-side var exists with the expected id and has 1 refcount (the
  // refcount on behalf of the plugin).
  int32_t expected_host_id =
      plugin().var_tracker().GetHostObject(instance_obj).value.as_id;
  Var* host_var = host().var_tracker().GetVar(expected_host_id);
  ASSERT_TRUE(host_var);
  EXPECT_EQ(
      1,
      host().var_tracker().GetRefCountForObject(GetPPVarNoAddRef(host_var)));

  // Call from the browser side to get the instance object.
  PP_Var host_pp_var = ppp_instance_private->GetInstanceObject(kInstance);
  EXPECT_EQ(instance_obj.type, host_pp_var.type);
  EXPECT_EQ(host_pp_var.value.as_id, expected_host_id);
  EXPECT_EQ(1, plugin().var_tracker().GetRefCountForObject(instance_obj));
  // A reference is passed to the browser, which we consume here.
  host().var_tracker().ReleaseVar(host_pp_var);
  EXPECT_EQ(1, host().var_tracker().GetRefCountForObject(host_pp_var));

  // The plugin is going away; generally, so will all references to its instance
  // object.
  host().var_tracker().ReleaseVar(host_pp_var);
  // Destroy the instance. DidDestroy above decrements the reference count for
  // instance_obj, so it should also be destroyed.
  ppp_instance->DidDestroy(kInstance);
  EXPECT_EQ(-1, plugin().var_tracker().GetRefCountForObject(instance_obj));
  EXPECT_EQ(-1, host().var_tracker().GetRefCountForObject(host_pp_var));
}

}  // namespace proxy
}  // namespace ppapi

