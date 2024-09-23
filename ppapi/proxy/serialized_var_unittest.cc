// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stdint.h>

#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/proxy/proxy_object_var.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

PP_Var MakeObjectVar(int64_t object_id) {
  PP_Var ret;
  ret.type = PP_VARTYPE_OBJECT;
  ret.value.as_id = object_id;
  return ret;
}

class SerializedVarTest : public PluginProxyTest {
 public:
  SerializedVarTest() {}
};

}  // namespace

// Tests output arguments in the plugin. This is when the host calls into the
// plugin and the plugin returns something via an out param, like an exception.
TEST_F(SerializedVarTest, PluginSerializedVarInOutParam) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the object param, we should be tracking it with no refcount, and
    // no messages sent.
    SerializedVarTestConstructor input(host_object);
    SerializedVarReceiveInput receive_input(input);
    plugin_object = receive_input.Get(plugin_dispatcher());
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    SerializedVar sv;
    {
      // The "OutParam" does its work in its destructor, it will write the
      // information to the SerializedVar we passed in the constructor.
      SerializedVarOutParam out_param(&sv);
      // An out-param needs to pass a reference to the caller, so it's the
      // responsibility of the plugin to bump the ref-count on an input
      // parameter.
      var_tracker().AddRefVar(plugin_object);
      EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
      // We should have informed the host that a reference was taken.
      EXPECT_EQ(1u, sink().message_count());
      *out_param.OutParam(plugin_dispatcher()) = plugin_object;
    }

    // The object should have transformed the plugin object back to the host
    // object ID. Nothing in the var tracker should have changed yet, and no
    // messages should have been sent.
    SerializedVarTestReader reader(sv);
    EXPECT_EQ(host_object.value.as_id, reader.GetVar().value.as_id);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(1u, sink().message_count());
  }

  // The out param should have done an "end receive caller owned" on the plugin
  // var serialization rules, which should have released the "track-with-no-
  // reference" count in the var tracker as well as the 1 reference we passed
  // back to the host, so the object should no longer be in the tracker. The
  // reference we added has been removed, so another message should be sent to
  // the host to tell it we're done with the object.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(2u, sink().message_count());
}

// Tests output strings in the plugin. This is when the host calls into the
// plugin with a string and the plugin returns it via an out param.
TEST_F(SerializedVarTest, PluginSerializedStringVarInOutParam) {
  ProxyAutoLock lock;
  PP_Var plugin_string;
  const std::string kTestString("elite");
  {
    // Receive the string param. We should track it with 1 refcount.
    SerializedVarTestConstructor input(kTestString);
    SerializedVarReceiveInput receive_input(input);
    plugin_string = receive_input.Get(plugin_dispatcher());
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_string));
    EXPECT_EQ(0u, sink().message_count());

    SerializedVar sv;
    {
      // The "OutParam" does its work in its destructor, it will write the
      // information to the SerializedVar we passed in the constructor.
      SerializedVarOutParam out_param(&sv);
      // An out-param needs to pass a reference to the caller, so it's the
      // responsibility of the plugin to bump the ref-count of an input
      // parameter.
      var_tracker().AddRefVar(plugin_string);
      EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_string));
      EXPECT_EQ(0u, sink().message_count());
      *out_param.OutParam(plugin_dispatcher()) = plugin_string;
    }

    // The SerializedVar should have set the string value internally. Nothing in
    // the var tracker should have changed yet, and no messages should have been
    // sent.
    SerializedVarTestReader reader(sv);
    //EXPECT_EQ(kTestString, *reader.GetTrackerStringPtr());
    EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_string));
    EXPECT_EQ(0u, sink().message_count());
  }
  // The reference the string had initially should be gone, and the reference we
  // passed to the host should also be gone, so the string should be removed.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_string));
  EXPECT_EQ(0u, sink().message_count());
}

// Tests receiving an argument and passing it back to the browser as an output
// parameter.
TEST_F(SerializedVarTest, PluginSerializedVarOutParam) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObjectVar(0x31337);

  // Start tracking this object in the plugin.
  PP_Var plugin_object = var_tracker().ReceiveObjectPassRef(
      host_object, plugin_dispatcher());
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));

  {
    SerializedVar sv;
    {
      // The "OutParam" does its work in its destructor, it will write the
      // information to the SerializedVar we passed in the constructor.
      SerializedVarOutParam out_param(&sv);
      *out_param.OutParam(plugin_dispatcher()) = plugin_object;
    }

    // The object should have transformed the plugin object back to the host
    // object ID. Nothing in the var tracker should have changed yet, and no
    // messages should have been sent.
    SerializedVarTestReader reader(sv);
    EXPECT_EQ(host_object.value.as_id, reader.GetVar().value.as_id);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());
  }

  // The out param should have done an "end send pass ref" on the plugin
  // var serialization rules, which should have in turn released the reference
  // in the var tracker. Since we only had one reference, this should have sent
  // a release to the browser.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // We don't bother validating that message since it's nontrivial and the
  // PluginVarTracker test has cases that cover that this message is correct.
}

// Tests the case that the plugin receives the same var twice as an input
// parameter (not passing ownership).
TEST_F(SerializedVarTest, PluginReceiveInput) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the first param, we should be tracking it with no refcount, and
    // no messages sent.
    SerializedVarTestConstructor input1(host_object);
    SerializedVarReceiveInput receive_input(input1);
    plugin_object = receive_input.Get(plugin_dispatcher());
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    // Receive the second param, it should be resolved to the same plugin
    // object and there should still be no refcount.
    SerializedVarTestConstructor input2(host_object);
    SerializedVarReceiveInput receive_input2(input2);
    PP_Var plugin_object2 = receive_input2.Get(plugin_dispatcher());
    EXPECT_EQ(plugin_object.value.as_id, plugin_object2.value.as_id);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    // Take a reference to the object, as if the plugin was using it, and then
    // release it, we should still be tracking the object since the
    // ReceiveInputs keep the "track_with_no_reference_count" alive until
    // they're destroyed.
    var_tracker().AddRefVar(plugin_object);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    var_tracker().ReleaseVar(plugin_object);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(2u, sink().message_count());
  }

  // Since we didn't keep any refs to the objects, it should have freed the
  // object.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
}

// Tests the case that the plugin receives the same vars twice as an input
// parameter (not passing ownership) within a vector.
TEST_F(SerializedVarTest, PluginVectorReceiveInput) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObjectVar(0x31337);

  std::vector<PP_Var> plugin_objects;
  std::vector<PP_Var> plugin_objects2;
  {
    // Receive the params. The object should be tracked with no refcount and
    // no messages sent. The string should is plugin-side only and should have
    // a reference-count of 1.
    std::vector<SerializedVar> input1;
    input1.push_back(SerializedVarTestConstructor(host_object));
    input1.push_back(SerializedVarTestConstructor("elite"));
    SerializedVarVectorReceiveInput receive_input(input1);
    uint32_t array_size = 0;
    PP_Var* plugin_objects_array =
        receive_input.Get(plugin_dispatcher(), &array_size);
    plugin_objects.insert(plugin_objects.begin(), plugin_objects_array,
                          plugin_objects_array + array_size);
    ASSERT_EQ(2u, array_size);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_objects[0]));
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_objects[1]));
    EXPECT_EQ(0u, sink().message_count());

    // Receive the second param, it should be resolved to the same plugin
    // object and there should still be no refcount.
    std::vector<SerializedVar> input2;
    input2.push_back(SerializedVarTestConstructor(host_object));
    input2.push_back(SerializedVarTestConstructor("elite"));
    SerializedVarVectorReceiveInput receive_input2(input2);
    uint32_t array_size2 = 0;
    PP_Var* plugin_objects_array2 =
        receive_input2.Get(plugin_dispatcher(), &array_size2);
    plugin_objects2.insert(plugin_objects2.begin(), plugin_objects_array2,
                           plugin_objects_array2 + array_size2);
    ASSERT_EQ(2u, array_size2);
    EXPECT_EQ(plugin_objects[0].value.as_id, plugin_objects2[0].value.as_id);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_objects[0]));
    // Strings get re-created with a new ID. We don't try to reuse strings in
    // the tracker, so the string should get a new ID.
    EXPECT_NE(plugin_objects[1].value.as_id, plugin_objects2[1].value.as_id);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_objects2[1]));
    EXPECT_EQ(0u, sink().message_count());

    // Take a reference to the object, as if the plugin was using it, and then
    // release it, we should still be tracking the object since the
    // ReceiveInputs keep the "track_with_no_reference_count" alive until
    // they're destroyed.
    var_tracker().AddRefVar(plugin_objects[0]);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_objects[0]));
    var_tracker().ReleaseVar(plugin_objects[0]);
    EXPECT_EQ(0, var_tracker().GetRefCountForObject(plugin_objects[0]));
    EXPECT_EQ(2u, sink().message_count());

    // Take a reference to a string and then release it. Make sure no messages
    // are sent.
    uint32_t old_message_count = static_cast<uint32_t>(sink().message_count());
    var_tracker().AddRefVar(plugin_objects[1]);
    EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_objects[1]));
    var_tracker().ReleaseVar(plugin_objects[1]);
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_objects[1]));
    EXPECT_EQ(old_message_count, sink().message_count());
  }

  // Since we didn't keep any refs to the objects or strings, so they should
  // have been freed.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_objects[0]));
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_objects[1]));
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_objects2[1]));
}

// Tests the browser sending a String var as a return value to make sure we
// ref-count the host side properly.
typedef HostProxyTest HostSerializedVarTest;
TEST_F(HostSerializedVarTest, PluginReceiveStringReturn) {
  {
    PP_Var string_var = StringVar::StringToPPVar("Hello");
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(string_var));
    GetDispatcher()->serialization_rules()->BeginSendPassRef(string_var);
    GetDispatcher()->serialization_rules()->EndSendPassRef(string_var);
    // It should be gone, so we should get -1 to indicate that.
    EXPECT_EQ(-1, var_tracker().GetRefCountForObject(string_var));
  }

  {
    // Note this is as little weird; we're testing the behavior of the host-
    // side of the proxy, but we use ProxyObjectVar, because this unit test
    // doesn't have access to stuff in content/renderer/pepper. The ref-counting
    // behavior should be the same, however. All we're really testing
    // is the code in ppapi/proxy (HostVarSerializationRules).
    scoped_refptr<Var> obj_var = new ProxyObjectVar(NULL, 1234);
    PP_Var obj_pp_var = obj_var->GetPPVar();
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(obj_pp_var));
    GetDispatcher()->serialization_rules()->BeginSendPassRef(obj_pp_var);
    GetDispatcher()->serialization_rules()->EndSendPassRef(obj_pp_var);
    // The host side for object vars always keeps 1 ref on behalf of the plugin.
    // See HostVarSerializationRules and PluginVarSerializationRules for an
    // explanation.
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(obj_pp_var));
    var_tracker().ReleaseVar(obj_pp_var);
  }
}

// Tests the plugin receiving a var as a return value from the browser
// two different times (passing ownership).
TEST_F(SerializedVarTest, PluginReceiveReturn) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the first param, we should be tracking it with a refcount of 1.
    SerializedVarTestConstructor input1(host_object);
    ReceiveSerializedVarReturnValue receive_input(input1);
    plugin_object = receive_input.Return(plugin_dispatcher());
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());

    // Receive the second param, it should be resolved to the same plugin
    // object and there should be a plugin refcount of 2. There should have
    // been an IPC message sent that released the duplicated ref in the browser
    // (so both of our refs are represented by one in the browser).
    SerializedVarTestConstructor input2(host_object);
    ReceiveSerializedVarReturnValue receive_input2(input2);
    PP_Var plugin_object2 = receive_input2.Return(plugin_dispatcher());
    EXPECT_EQ(plugin_object.value.as_id, plugin_object2.value.as_id);
    EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(1u, sink().message_count());
  }

  // The ReceiveSerializedVarReturnValue destructor shouldn't have affected
  // the refcount or sent any messages.
  EXPECT_EQ(2, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // Manually release one refcount, it shouldn't have sent any more messages.
  var_tracker().ReleaseVar(plugin_object);
  EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());

  // Manually release the last refcount, it should have freed it and sent a
  // release message to the browser.
  var_tracker().ReleaseVar(plugin_object);
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(2u, sink().message_count());
}

// Returns a value from the browser to the plugin, then return that one ref
// back to the browser.
TEST_F(SerializedVarTest, PluginReturnValue) {
  ProxyAutoLock lock;
  PP_Var host_object = MakeObjectVar(0x31337);

  PP_Var plugin_object;
  {
    // Receive the param in the plugin.
    SerializedVarTestConstructor input1(host_object);
    ReceiveSerializedVarReturnValue receive_input(input1);
    plugin_object = receive_input.Return(plugin_dispatcher());
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
    EXPECT_EQ(0u, sink().message_count());
  }

  {
    // Now return to the browser.
    SerializedVar output;
    SerializedVarReturnValue return_output(&output);
    return_output.Return(plugin_dispatcher(), plugin_object);

    // The ref in the plugin should be alive until the ReturnValue goes out of
    // scope, since the release needs to be after the browser processes the
    // message.
    EXPECT_EQ(1, var_tracker().GetRefCountForObject(plugin_object));
  }

  // When the ReturnValue object goes out of scope, it should have sent a
  // release message to the browser.
  EXPECT_EQ(-1, var_tracker().GetRefCountForObject(plugin_object));
  EXPECT_EQ(1u, sink().message_count());
}

}  // namespace proxy
}  // namespace ppapi
