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
      bool js_error_reporting_enabled = false,
      bool should_crash_on_js_error = false)
      : extension_id_(std::move(extension_id)),
        frame_binder_(std::move(frame_binder)),
        sw_binder_(std::move(sw_binder)),
        js_error_reporting_enabled_(js_error_reporting_enabled),
        should_crash_on_js_error_(should_crash_on_js_error) {}
  ~TestBinderProvider() override = default;

  ExtensionId GetExtensionId() const override { return extension_id_; }

  bool IsJsErrorReportingEnabled() const override {
    return js_error_reporting_enabled_;
  }

  bool ShouldCrashOnJsErrorInDevelopmentBuild() const override {
    return should_crash_on_js_error_;
  }

  void PopulateFrameBinders(
      mojo::BinderMapWithContext<content::RenderFrameHost*>& binder_map,
      content::RenderFrameHost* render_frame_host,
      const Extension* extension) override {
    if (frame_binder_) {
      binder_map.Add<TestInterface>(frame_binder_);
    }
  }

  void PopulateServiceWorkerBinders(
      mojo::BinderMapWithContext<const content::ServiceWorkerVersionBaseInfo&>&
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
  bool js_error_reporting_enabled_ = false;
  bool should_crash_on_js_error_ = false;
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
    return ExtensionMojoBinderRegistryFactory::GetForBrowserContext(
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
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

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
  registry()->PopulateServiceWorkerBinders(&binder_map, nullptr,
                                           extension.get());

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
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

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
  registry()->PopulateFrameBinders(&binder_map, nullptr, extension.get());

  mojo::GenericPendingReceiver receiver(TestInterface::Name_,
                                        mojo::MessagePipe().handle0);
  EXPECT_FALSE(binder_map.TryBind(nullptr, &receiver));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(ExtensionMojoBinderRegistryTest, IsMojoJsEnabled) {
  EXPECT_FALSE(registry()->IsMojoJsEnabled(nullptr));

  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("Component Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();
  scoped_refptr<const Extension> unpacked_extension =
      ExtensionBuilder("Unpacked Extension")
          .SetLocation(mojom::ManifestLocation::kUnpacked)
          .Build();

  EXPECT_FALSE(registry()->IsMojoJsEnabled(component_extension.get()));
  EXPECT_FALSE(registry()->IsMojoJsEnabled(unpacked_extension.get()));

  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      component_extension->id(), base::NullCallback(), base::NullCallback()));

  EXPECT_TRUE(registry()->IsMojoJsEnabled(component_extension.get()));
  EXPECT_FALSE(registry()->IsMojoJsEnabled(unpacked_extension.get()));
}

TEST_F(ExtensionMojoBinderRegistryTest, IsJsErrorReportingEnabled) {
  EXPECT_FALSE(registry()->IsJsErrorReportingEnabled(nullptr));
  EXPECT_FALSE(registry()->ShouldCrashOnJsErrorInDevelopmentBuild(nullptr));

  scoped_refptr<const Extension> component_extension =
      ExtensionBuilder("Component Extension")
          .SetLocation(mojom::ManifestLocation::kComponent)
          .Build();

  EXPECT_FALSE(
      registry()->IsJsErrorReportingEnabled(component_extension.get()));
  EXPECT_FALSE(registry()->ShouldCrashOnJsErrorInDevelopmentBuild(
      component_extension.get()));

  RegisterTestProvider(std::make_unique<TestBinderProvider>(
      component_extension->id(), base::NullCallback(), base::NullCallback(),
      /*js_error_reporting_enabled=*/true,
      /*should_crash_on_js_error=*/true));

  EXPECT_TRUE(registry()->IsJsErrorReportingEnabled(component_extension.get()));
  if (version_info::IsOfficialBuild()) {
    EXPECT_FALSE(registry()->ShouldCrashOnJsErrorInDevelopmentBuild(
        component_extension.get()));
  } else {
    EXPECT_TRUE(registry()->ShouldCrashOnJsErrorInDevelopmentBuild(
        component_extension.get()));

    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableCrashOnComponentExtensionJsError);
    EXPECT_FALSE(registry()->ShouldCrashOnJsErrorInDevelopmentBuild(
        component_extension.get()));
  }
}

}  // namespace extensions
