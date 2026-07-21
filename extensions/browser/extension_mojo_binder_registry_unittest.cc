// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_mojo_binder_registry.h"

#include <memory>
#include <string_view>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/test_future.h"
#include "content/public/browser/service_worker_version_base_info.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/mojom/app_window.mojom.h"
#include "extensions/common/mojom/keep_alive.mojom.h"
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
          sw_binder)
      : extension_id_(std::move(extension_id)),
        frame_binder_(std::move(frame_binder)),
        sw_binder_(std::move(sw_binder)) {}
  ~TestBinderProvider() override = default;

  ExtensionId GetExtensionId() const override { return extension_id_; }

  void PopulateFrameBinders(
      ExtensionBinderMap<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) override {
    if (frame_binder_) {
      binder_map.Add<TestInterface>(frame_binder_);
    }
  }

  void PopulateServiceWorkerBinders(
      ExtensionBinderMap<const content::ServiceWorkerVersionBaseInfo&>&
          binder_map,
      content::BrowserContext* browser_context,
      const Extension* extension) override {
    if (sw_binder_) {
      binder_map.Add<TestInterface>(
          base::BindRepeating(sw_binder_, browser_context));
    }
  }

 private:
  ExtensionId extension_id_;
  base::RepeatingCallback<void(content::RenderFrameHost*,
                               mojo::PendingReceiver<TestInterface>)>
      frame_binder_;
  base::RepeatingCallback<void(content::BrowserContext*,
                               const content::ServiceWorkerVersionBaseInfo&,
                               mojo::PendingReceiver<TestInterface>)>
      sw_binder_;
};

}  // namespace

class ExtensionMojoBinderRegistryTest : public testing::Test {
 public:
  ExtensionMojoBinderRegistryTest() = default;
  ~ExtensionMojoBinderRegistryTest() override = default;

  void SetUp() override {
    registry()->ClearProvidersForTesting();
    registry()->SetBypassAllowlistForTesting(false);
  }

  void TearDown() override {
    registry()->ClearProvidersForTesting();
    registry()->SetBypassAllowlistForTesting(false);
  }

  ExtensionMojoBinderRegistry* registry() {
    return ExtensionMojoBinderRegistry::GetInstance();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(ExtensionMojoBinderRegistryTest, FrameBinderInvokedWhenAllowlisted) {
  registry()->SetBypassAllowlistForTesting(true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  registry()->RegisterProvider(std::make_unique<TestBinderProvider>(
      extension->id(), future.GetRepeatingCallback(), base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_TRUE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_TRUE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest, FrameBinderRejectedWhenNotAllowlisted) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  registry()->RegisterProvider(std::make_unique<TestBinderProvider>(
      extension->id(), future.GetRepeatingCallback(), base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest,
       ServiceWorkerBinderInvokedWhenAllowlisted) {
  registry()->SetBypassAllowlistForTesting(true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::BrowserContext*,
                         content::ServiceWorkerVersionBaseInfo,
                         mojo::PendingReceiver<TestInterface>>
      future;
  registry()->RegisterProvider(std::make_unique<TestBinderProvider>(
      extension->id(), base::NullCallback(),
      future.GetRepeatingCallback<content::BrowserContext*,
                                  const content::ServiceWorkerVersionBaseInfo&,
                                  mojo::PendingReceiver<TestInterface>>()));

  mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>
      binder_map;
  registry()->PopulateServiceWorkerBinders(&binder_map, nullptr,
                                           extension.get());

  content::ServiceWorkerVersionBaseInfo info;
  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_TRUE(binder_map.TryBind(info, &receiver));
  EXPECT_TRUE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest,
       ServiceWorkerBinderRejectedWhenNotAllowlisted) {
  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::BrowserContext*,
                         content::ServiceWorkerVersionBaseInfo,
                         mojo::PendingReceiver<TestInterface>>
      future;
  registry()->RegisterProvider(std::make_unique<TestBinderProvider>(
      extension->id(), base::NullCallback(),
      future.GetRepeatingCallback<content::BrowserContext*,
                                  const content::ServiceWorkerVersionBaseInfo&,
                                  mojo::PendingReceiver<TestInterface>>()));

  mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>
      binder_map;
  registry()->PopulateServiceWorkerBinders(&binder_map, nullptr,
                                           extension.get());

  content::ServiceWorkerVersionBaseInfo info;
  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(info, &receiver));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest, RejectedByRegistryWhenNotComponent) {
  registry()->SetBypassAllowlistForTesting(true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension").Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  registry()->RegisterProvider(std::make_unique<TestBinderProvider>(
      extension->id(), future.GetRepeatingCallback(), base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest,
       RejectedByRegistryWhenExtensionIdMismatch) {
  registry()->SetBypassAllowlistForTesting(true);

  scoped_refptr<const Extension> extension =
      ExtensionBuilder("Test Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  base::test::TestFuture<content::RenderFrameHost*,
                         mojo::PendingReceiver<TestInterface>>
      future;
  registry()->RegisterProvider(std::make_unique<TestBinderProvider>(
      "different-extension-id", future.GetRepeatingCallback(),
      base::NullCallback()));

  mojo::BinderMapWithContext<content::RenderFrameHost*> binder_map;
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_FALSE(future.IsReady());
}

}  // namespace extensions
