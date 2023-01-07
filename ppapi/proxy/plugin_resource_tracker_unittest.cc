// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/plugin_resource_tracker.h"

#include "base/process/process.h"
#include "ppapi/proxy/mock_resource.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/proxy_lock.h"

namespace ppapi {
namespace proxy {

namespace {

// Object so we know when a resource has been released.
class TrackedMockResource : public MockResource {
 public:
  TrackedMockResource(const HostResource& serialized)
      : MockResource(serialized) {
    tracked_alive_count++;
  }
  ~TrackedMockResource() {
    tracked_alive_count--;
  }

  static int tracked_alive_count;
};

int TrackedMockResource::tracked_alive_count = 0;

}  // namespace

class PluginResourceTrackerTest : public PluginProxyTest {
 public:
  PluginResourceTrackerTest() {}
  ~PluginResourceTrackerTest() {}
};

TEST_F(PluginResourceTrackerTest, PluginResourceForHostResource) {
  ProxyAutoLock lock;

  PP_Resource host_resource = 0x5678;

  HostResource serialized;
  serialized.SetHostResource(pp_instance(), host_resource);

  // When we haven't added an object, the return value should be 0.
  EXPECT_EQ(0, resource_tracker().PluginResourceForHostResource(serialized));

  EXPECT_EQ(0, TrackedMockResource::tracked_alive_count);
  TrackedMockResource* object = new TrackedMockResource(serialized);
  EXPECT_EQ(1, TrackedMockResource::tracked_alive_count);
  PP_Resource plugin_resource = object->GetReference();

  // Now that the object has been added, the return value should be the plugin
  // resource ID we already got.
  EXPECT_EQ(plugin_resource,
            resource_tracker().PluginResourceForHostResource(serialized));

  // Releasing the resource should have freed it.
  resource_tracker().ReleaseResource(plugin_resource);
  EXPECT_EQ(0, TrackedMockResource::tracked_alive_count);
  EXPECT_EQ(0, resource_tracker().PluginResourceForHostResource(serialized));
}

}  // namespace proxy
}  // namespace ppapi
