// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/plugin_data.h"

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/plugins/plugin_registry.mojom-blink.h"
#include "third_party/blink/renderer/platform/testing/testing_platform_support.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "url/url_util.h"

using testing::Eq;
using testing::Property;

namespace blink {
namespace {

class MockPluginRegistry : public mojom::blink::PluginRegistry {
 public:
  void GetPlugins(bool refresh,
                  const scoped_refptr<const SecurityOrigin>& origin,
                  GetPluginsCallback callback) override {
    DidGetPlugins(refresh, *origin);
    std::move(callback).Run(Vector<mojom::blink::PluginInfoPtr>());
  }

  MOCK_METHOD2(DidGetPlugins, void(bool, const SecurityOrigin&));
};

// Regression test for https://crbug.com/862282
TEST(PluginDataTest, NonStandardUrlSchemeRequestsPluginsWithUniqueOrigin) {
  ScopedTestingPlatformSupport<TestingPlatformSupport> support;
  // Create a scheme that's local but nonstandard, as in bug 862282.
  url::AddLocalScheme("nonstandard-862282");
  SchemeRegistry::RegisterURLSchemeAsLocal("nonstandard-862282");

  MockPluginRegistry mock_plugin_registry;
  mojo::Receiver<mojom::blink::PluginRegistry> registry_receiver(
      &mock_plugin_registry);
  TestingPlatformSupport::ScopedOverrideMojoInterface override_plugin_registry(
      WTF::BindRepeating(
          [](mojo::Receiver<mojom::blink::PluginRegistry>* registry_receiver,
             const char* interface, mojo::ScopedMessagePipeHandle pipe) {
            if (!strcmp(interface, mojom::blink::PluginRegistry::Name_)) {
              registry_receiver->Bind(
                  mojo::PendingReceiver<mojom::blink::PluginRegistry>(
                      std::move(pipe)));
              return;
            }
          },
          WTF::Unretained(&registry_receiver)));

  EXPECT_CALL(
      mock_plugin_registry,
      DidGetPlugins(false, Property(&SecurityOrigin::IsOpaque, Eq(false))));

  scoped_refptr<SecurityOrigin> non_standard_origin =
      SecurityOrigin::CreateFromString("nonstandard-862282:foo/bar");
  EXPECT_FALSE(non_standard_origin->IsOpaque());
  auto* plugin_data = MakeGarbageCollected<PluginData>();
  plugin_data->UpdatePluginList(non_standard_origin.get());
}

}  // namespace
}  // namespace blink
