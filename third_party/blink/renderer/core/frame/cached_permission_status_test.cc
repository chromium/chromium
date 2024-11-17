// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/cached_permission_status.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/testing/page_test_base.h"

namespace blink {

using mojom::blink::PermissionDescriptor;
using mojom::blink::PermissionDescriptorPtr;
using mojom::blink::PermissionName;
using mojom::blink::PermissionObserver;
using mojom::blink::PermissionStatus;

namespace {

PermissionDescriptorPtr CreatePermissionDescriptor(PermissionName name) {
  auto descriptor = PermissionDescriptor::New();
  descriptor->name = name;
  return descriptor;
}

Vector<PermissionDescriptorPtr> CreatePermissionDescriptors(
    const AtomicString& permissions_string) {
  SpaceSplitString permissions(permissions_string);
  Vector<PermissionDescriptorPtr> permission_descriptors;

  for (const auto& permission : permissions) {
    if (permission == "geolocation") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::GEOLOCATION));
    } else if (permission == "camera") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::VIDEO_CAPTURE));
    } else if (permission == "microphone") {
      permission_descriptors.push_back(
          CreatePermissionDescriptor(PermissionName::AUDIO_CAPTURE));
    }
  }

  return permission_descriptors;
}

}  // namespace

class MockHTMLPermissionElement
    : public GarbageCollected<MockHTMLPermissionElement>,
      public CachedPermissionStatus::Client {
 public:
  MockHTMLPermissionElement() = default;

  ~MockHTMLPermissionElement() override = default;

  void OnPermissionStatusInitialized(
      HashMap<PermissionName, PermissionStatus> map) override {}

  void Trace(Visitor* visitor) const override {}
};

class CachedPermissionStatusTest : public PageTestBase {
 public:
  CachedPermissionStatusTest() = default;

  CachedPermissionStatusTest(const CachedPermissionStatusTest&) = delete;
  CachedPermissionStatusTest& operator=(const CachedPermissionStatusTest&) =
      delete;

  void SetUp() override {
    PageTestBase::SetUp();
    CachedPermissionStatus::From(GetDocument().domWindow())
        ->SetPermissionStatusMap(HashMap<PermissionName, PermissionStatus>(
            {{PermissionName::VIDEO_CAPTURE, PermissionStatus::ASK},
             {PermissionName::AUDIO_CAPTURE, PermissionStatus::ASK},
             {PermissionName::GEOLOCATION, PermissionStatus::ASK}}));
  }

  bool HasClient(PermissionName permission,
                 CachedPermissionStatus::Client* client) const {
    CachedPermissionStatus* cache =
        CachedPermissionStatus::From(GetDocument().domWindow());
    const auto& clients = cache->GetClientsForTesting();
    auto it = clients.find(permission);
    if (it == clients.end()) {
      return false;
    }

    const auto& client_set = it->value;
    return client_set.find(client) != client_set.end();
  }

  bool HasPermisionObserver(PermissionName permission) const {
    CachedPermissionStatus* cache =
        CachedPermissionStatus::From(GetDocument().domWindow());
    const auto& permission_to_receivers_map =
        cache->GetPermissionToReceiversMapForTesting();
    auto it = permission_to_receivers_map.find(permission);
    if (it == permission_to_receivers_map.end()) {
      return false;
    }
    mojo::ReceiverId id = it->value;
    auto& permission_observer_receivers =
        cache->GetPermissionObserverReceiversForTesting();
    return permission_observer_receivers.HasReceiver(id);
  }
};

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_RegisterClient DISABLED_RegisterClient
#else
#define MAYBE_RegisterClient RegisterClient
#endif
TEST_F(CachedPermissionStatusTest, MAYBE_RegisterClient) {
  auto* client1 = MakeGarbageCollected<MockHTMLPermissionElement>();
  CachedPermissionStatus* cache =
      CachedPermissionStatus::From(GetDocument().domWindow());
  cache->RegisterClient(
      client1, CreatePermissionDescriptors(AtomicString("geolocation")));
  EXPECT_TRUE(HasClient(PermissionName::GEOLOCATION, client1));
  EXPECT_TRUE(HasPermisionObserver(PermissionName::GEOLOCATION));
  auto* client2 = MakeGarbageCollected<MockHTMLPermissionElement>();
  cache->RegisterClient(
      client2, CreatePermissionDescriptors(AtomicString("geolocation")));
  EXPECT_TRUE(HasClient(PermissionName::GEOLOCATION, client2));
  auto* client3 = MakeGarbageCollected<MockHTMLPermissionElement>();
  cache->RegisterClient(client3,
                        CreatePermissionDescriptors(AtomicString("camera")));
  EXPECT_TRUE(HasClient(PermissionName::VIDEO_CAPTURE, client3));
  EXPECT_TRUE(HasPermisionObserver(PermissionName::VIDEO_CAPTURE));
  auto clients = cache->GetClientsForTesting();
  {
    auto it = clients.find(PermissionName::GEOLOCATION);
    EXPECT_TRUE(it != clients.end());
    EXPECT_EQ(it->value.size(), 2u);
  }
  {
    auto it = clients.find(PermissionName::VIDEO_CAPTURE);
    EXPECT_TRUE(it != clients.end());
    EXPECT_EQ(it->value.size(), 1u);
  }
}

#if BUILDFLAG(IS_ANDROID)
#define MAYBE_UnregisterClientRemoveObserver \
  DISABLED_UnregisterClientRemoveObserver
#else
#define MAYBE_UnregisterClientRemoveObserver UnregisterClientRemoveObserver
#endif
TEST_F(CachedPermissionStatusTest, MAYBE_UnregisterClientRemoveObserver) {
  auto* client1 = MakeGarbageCollected<MockHTMLPermissionElement>();
  CachedPermissionStatus* cache =
      CachedPermissionStatus::From(GetDocument().domWindow());
  cache->RegisterClient(
      client1, CreatePermissionDescriptors(AtomicString("geolocation")));
  EXPECT_TRUE(HasClient(PermissionName::GEOLOCATION, client1));
  EXPECT_TRUE(HasPermisionObserver(PermissionName::GEOLOCATION));
  auto* client2 = MakeGarbageCollected<MockHTMLPermissionElement>();
  cache->RegisterClient(
      client2, CreatePermissionDescriptors(AtomicString("geolocation")));
  EXPECT_TRUE(HasClient(PermissionName::GEOLOCATION, client2));
  auto* client3 = MakeGarbageCollected<MockHTMLPermissionElement>();
  cache->RegisterClient(client3,
                        CreatePermissionDescriptors(AtomicString("camera")));
  EXPECT_TRUE(HasClient(PermissionName::VIDEO_CAPTURE, client3));
  EXPECT_TRUE(HasPermisionObserver(PermissionName::VIDEO_CAPTURE));

  cache->UnregisterClient(
      client2, CreatePermissionDescriptors(AtomicString("geolocation")));
  EXPECT_TRUE(HasPermisionObserver(PermissionName::GEOLOCATION));
  EXPECT_TRUE(HasPermisionObserver(PermissionName::VIDEO_CAPTURE));
  cache->UnregisterClient(
      client1, CreatePermissionDescriptors(AtomicString("geolocation")));
  EXPECT_FALSE(HasPermisionObserver(PermissionName::GEOLOCATION));
  cache->UnregisterClient(client3,
                          CreatePermissionDescriptors(AtomicString("camera")));
  EXPECT_FALSE(HasPermisionObserver(PermissionName::VIDEO_CAPTURE));
}
}  // namespace blink
