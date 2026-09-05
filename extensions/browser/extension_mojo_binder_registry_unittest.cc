// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_mojo_binder_registry.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "base/types/pass_key.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "extensions/browser/extension_mojo_binder_registry_factory.h"
#include "extensions/browser/extensions_test.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/keep_alive.mojom.h"
#include "extensions/common/switches.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

using TestInterface = extensions::KeepAlive;

class TestBinderProvider : public ExtensionMojoBinderProvider {
 public:
  TestBinderProvider(
      ExtensionId extension_id,
      base::RepeatingCallback<void(content::RenderFrameHost*,
                                   mojo::PendingReceiver<TestInterface>)>
          frame_binder,
      base::RepeatingCallback<void(content::BrowserContext*,
                                   const content::ServiceWorkerVersionBaseInfo&,
                                   mojo::PendingReceiver<TestInterface>)>
          sw_binder,
      bool is_mojo_js_enabled_for_frame = false,
      bool is_mojo_js_enabled_for_service_worker = false)
      : ExtensionMojoBinderProvider(std::move(extension_id)),
        frame_binder_(std::move(frame_binder)),
        sw_binder_(std::move(sw_binder)),
        is_mojo_js_enabled_for_frame_(is_mojo_js_enabled_for_frame),
        is_mojo_js_enabled_for_service_worker_(
            is_mojo_js_enabled_for_service_worker) {}
  ~TestBinderProvider() override = default;

  bool IsMojoJsEnabledForFrame() const override {
    return is_mojo_js_enabled_for_frame_;
  }

  bool IsMojoJsEnabledForServiceWorker() const override {
    return is_mojo_js_enabled_for_service_worker_;
  }
  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension& extension) override {
    if (frame_binder_) {
      binder_map.Add<TestInterface>(frame_binder_);
    }
  }

  void PopulateServiceWorkerBinders(
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>&
          binder_map,
      content::BrowserContext* browser_context,
      const Extension& extension) override {
    if (sw_binder_) {
      binder_map.Add<TestInterface>(
          base::BindRepeating(sw_binder_, browser_context));
    }
  }

 private:
  base::RepeatingCallback<void(content::RenderFrameHost*,
                               mojo::PendingReceiver<TestInterface>)>
      frame_binder_;
  base::RepeatingCallback<void(content::BrowserContext*,
                               const content::ServiceWorkerVersionBaseInfo&,
                               mojo::PendingReceiver<TestInterface>)>
      sw_binder_;
  const bool is_mojo_js_enabled_for_frame_ = false;
  const bool is_mojo_js_enabled_for_service_worker_ = false;
};

}  // namespace

class ExtensionMojoBinderRegistryTest : public ExtensionsTest {
 public:
  ExtensionMojoBinderRegistryTest() = default;
  ~ExtensionMojoBinderRegistryTest() override = default;

  void SetUp() override {
    ExtensionsTest::SetUp();
    registry()->ClearProvidersForTesting();
  }

  void TearDown() override {
    registry()->ClearProvidersForTesting();
    ExtensionsTest::TearDown();
  }

  ExtensionMojoBinderRegistry* registry() {
    return ExtensionMojoBinderRegistryFactory::GetOrCreateForBrowserContext(
        browser_context());
  }

  void RegisterTestProvider(
      std::unique_ptr<ExtensionMojoBinderProvider> provider) {
    registry()->RegisterProvider(
        base::PassKey<ExtensionMojoBinderRegistryTest>(), std::move(provider));
  }
};

TEST_F(ExtensionMojoBinderRegistryTest, FrameBinderInvoked) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      extension->id(), future.GetRepeatingCallback(), base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, *extension);

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_TRUE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_TRUE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest, ServiceWorkerBinderInvoked) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::BrowserContext*,
                         content::ServiceWorkerVersionBaseInfo,
                         mojo::PendingReceiver<TestInterface>>
      future;
  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      extension->id(), base::NullCallback(),
      future.GetRepeatingCallback<content::BrowserContext*,
                                  const content::ServiceWorkerVersionBaseInfo&,
                                  mojo::PendingReceiver<TestInterface>>()));

  mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>
      binder_map;
  registry()->PopulateServiceWorkerBinders(&binder_map, nullptr, *extension);

  content::ServiceWorkerVersionBaseInfo info;
  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_TRUE(binder_map.TryBind(info, &receiver));
  EXPECT_TRUE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest, RejectedByRegistryWhenNotComponent) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension").Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      extension->id(), future.GetRepeatingCallback(), base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, *extension);

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest,
       RejectedByRegistryWhenExtensionIdMismatch) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      "different-extension-id", future.GetRepeatingCallback(),
      base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, *extension);

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest, IsMojoJsEnabledForFrame) {
  scoped_refptr<const Extension> component_extension_1 =
      ExtensionBuilder("Component Extension 1")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  scoped_refptr<const Extension> component_extension_2 =
      ExtensionBuilder("Component Extension 2")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  scoped_refptr<const Extension> unpacked_extension =
      ExtensionBuilder("Unpacked Extension")
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .Build();

  EXPECT_FALSE(registry()->IsMojoJsEnabledForFrame(*component_extension_1));
  EXPECT_FALSE(registry()->IsMojoJsEnabledForFrame(*component_extension_2));
  EXPECT_FALSE(registry()->IsMojoJsEnabledForFrame(*unpacked_extension));

  // Provider with default settings (Mojo JS disabled for frame).
  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      component_extension_1->id(), base::NullCallback(), base::NullCallback()));

  // Provider with frame Mojo JS explicitly enabled.
  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      component_extension_2->id(), base::NullCallback(), base::NullCallback(),
      /*is_mojo_js_enabled_for_frame=*/true));

  EXPECT_FALSE(registry()->IsMojoJsEnabledForFrame(*component_extension_1));
  EXPECT_TRUE(registry()->IsMojoJsEnabledForFrame(*component_extension_2));
  EXPECT_FALSE(registry()->IsMojoJsEnabledForFrame(*unpacked_extension));
}

TEST_F(ExtensionMojoBinderRegistryTest, IsMojoJsEnabledForServiceWorker) {
  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("Component Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  scoped_refptr<const Extension> unpacked_extension =
      ExtensionBuilder("Unpacked Extension")
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .Build();

  EXPECT_FALSE(
      registry()->IsMojoJsEnabledForServiceWorker(*component_extension));
  EXPECT_FALSE(
      registry()->IsMojoJsEnabledForServiceWorker(*unpacked_extension));

  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      component_extension->id(), base::NullCallback(), base::NullCallback(),
      /*is_mojo_js_enabled_for_frame=*/false,
      /*is_mojo_js_enabled_for_service_worker=*/true));

  EXPECT_FALSE(registry()->IsMojoJsEnabledForFrame(*component_extension));
  EXPECT_TRUE(
      registry()->IsMojoJsEnabledForServiceWorker(*component_extension));
  EXPECT_FALSE(
      registry()->IsMojoJsEnabledForServiceWorker(*unpacked_extension));
}
}  // namespace extensions
