// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/media_interface_proxy.h"

#include <memory>
#include <optional>
#include <string>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "content/browser/media/cdm_registry_impl.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/common/pseudonymization_salt.h"
#include "content/public/browser/media_player_id.h"
#include "content/public/common/cdm_info.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/cdm_capability.h"
#include "media/base/cdm_config.h"
#include "media/base/media_switches.h"
#include "media/mojo/mojom/content_decryption_module.mojom.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/media_log.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "sandbox/policy/switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

const char kTestKeySystem[] = "org.chromium.externalclearkey.mediafoundation";

}  // namespace

class MediaInterfaceProxyTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    SetPseudonymizationSalt(12345);
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        sandbox::policy::switches::kNoSandbox);

    // Register a hardware-secure CDM in CdmRegistry.
    CdmRegistryImpl::GetInstance()->RegisterCdm(CdmInfo(
        kTestKeySystem, CdmInfo::Robustness::kHardwareSecure, std::nullopt,
        /*supports_sub_key_systems=*/false, "Test Hardware Secure CDM",
        media::CdmType(), base::FilePath(FILE_PATH_LITERAL("test_cdm_path"))));
  }

  void TearDown() override {
    ResetSaltForTesting();
    RenderViewHostTestHarness::TearDown();
  }

#if BUILDFLAG(IS_WIN)
  void SetMediaFoundationInterfaceFactoryRemote(
      mojo::PendingRemote<media::mojom::InterfaceFactory> factory_remote) {
    auto* proxy =
        MediaInterfaceProxy::GetOrCreateForCurrentDocument(main_rfh());
    proxy->mf_interface_factory_remote_.Bind(std::move(factory_remote));
  }
#endif

  mojo::Remote<media::mojom::InterfaceFactory> GetMediaInterfaceFactory() {
    mojo::Remote<media::mojom::InterfaceFactory> factory;
    static_cast<RenderFrameHostImpl*>(main_rfh())
        ->BindMediaInterfaceFactoryReceiver(
            factory.BindNewPipeAndPassReceiver());
    return factory;
  }

  media::CdmConfig CreateHwSecureCdmConfig() {
    media::CdmConfig config;
    config.key_system = kTestKeySystem;
    config.allow_distinctive_identifier = true;
    config.allow_persistent_state = true;
    config.use_hw_secure_codecs = true;
    return config;
  }
};

class MediaInterfaceProxyOffTheRecordTest : public MediaInterfaceProxyTest {
 protected:
  std::unique_ptr<BrowserContext> CreateBrowserContext() override {
    auto browser_context = std::make_unique<TestBrowserContext>();
    browser_context->set_is_off_the_record(true);
    return browser_context;
  }
};

#if BUILDFLAG(IS_WIN)
TEST_F(MediaInterfaceProxyOffTheRecordTest,
       CreateCdm_MediaFoundation_OffTheRecord) {
  auto factory = GetMediaInterfaceFactory();
  ASSERT_TRUE(factory.is_bound());

  base::MockCallback<media::mojom::InterfaceFactory::CreateCdmCallback> mock_cb;
  std::optional<media::CreateCdmStatus> created_status;
  base::RunLoop run_loop;

  EXPECT_CALL(mock_cb, Run(testing::_, testing::_, testing::_))
      .WillOnce([&](mojo::PendingRemote<media::mojom::ContentDecryptionModule>,
                    media::mojom::CdmContextPtr,
                    media::CreateCdmStatus status) {
        created_status = status;
        run_loop.Quit();
      });

  factory->CreateCdm(CreateHwSecureCdmConfig(), mock_cb.Get());
  run_loop.Run();

  // In off-the-record contexts, hardware-secure MediaFoundation CDM creation
  // must be refused to prevent leaking regular profile DRM identity and writing
  // persistent storage to the regular profile directory.
  ASSERT_TRUE(created_status.has_value());
  EXPECT_EQ(created_status.value(), media::CreateCdmStatus::kCdmNotSupported);
}

TEST_F(MediaInterfaceProxyTest, CreateCdm_MediaFoundation_InvalidConfig) {
  auto factory = GetMediaInterfaceFactory();
  ASSERT_TRUE(factory.is_bound());

  media::CdmConfig config = CreateHwSecureCdmConfig();
  config.allow_persistent_state = false;

  base::MockCallback<media::mojom::InterfaceFactory::CreateCdmCallback> mock_cb;
  std::optional<media::CreateCdmStatus> created_status;
  base::RunLoop run_loop;

  EXPECT_CALL(mock_cb, Run(testing::_, testing::_, testing::_))
      .WillOnce([&](mojo::PendingRemote<media::mojom::ContentDecryptionModule>,
                    media::mojom::CdmContextPtr,
                    media::CreateCdmStatus status) {
        created_status = status;
        run_loop.Quit();
      });

  factory->CreateCdm(config, mock_cb.Get());
  run_loop.Run();

  ASSERT_TRUE(created_status.has_value());
  EXPECT_EQ(created_status.value(), media::CreateCdmStatus::kInvalidCdmConfig);
}

TEST_F(MediaInterfaceProxyTest,
       CreateMediaFoundationRenderer_NoGrantWithoutContext) {
  mojo::PendingRemote<media::mojom::InterfaceFactory> dummy_factory_remote;
  auto dummy_factory_receiver =
      dummy_factory_remote.InitWithNewPipeAndPassReceiver();
  SetMediaFoundationInterfaceFactoryRemote(std::move(dummy_factory_remote));

  auto factory = GetMediaInterfaceFactory();
  ASSERT_TRUE(factory.is_bound());

  mojo::PendingRemote<media::mojom::MediaLog> media_log_remote;
  auto media_log_receiver = media_log_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<media::mojom::Renderer> renderer_remote;
  auto renderer_receiver = renderer_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<media::mojom::MediaFoundationRendererExtension>
      extension_remote;

  // A renderer should NOT be granted audibility bypass authorization when
  // calling CreateMediaFoundationRenderer without an established
  // MediaFoundation playback context (no CDM created, clear playback disabled).
  factory->CreateMediaFoundationRenderer(
      std::move(media_log_remote), std::move(renderer_receiver),
      extension_remote.InitWithNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  MediaPlayerId player_id(main_rfh()->GetGlobalId(), 1);
  EXPECT_FALSE(AudibilityBypassTracker::ClaimGrant(player_id));
}

// TODO(crbug.com/564789121): Failing on Win11 ARM64.
#if BUILDFLAG(IS_WIN) && defined(ARCH_CPU_ARM64)
#define MAYBE_CreateMediaFoundationRenderer_GrantWithProtectedContext \
  DISABLED_CreateMediaFoundationRenderer_GrantWithProtectedContext
#else
#define MAYBE_CreateMediaFoundationRenderer_GrantWithProtectedContext \
  CreateMediaFoundationRenderer_GrantWithProtectedContext
#endif
TEST_F(MediaInterfaceProxyTest,
       MAYBE_CreateMediaFoundationRenderer_GrantWithProtectedContext) {
  mojo::PendingRemote<media::mojom::InterfaceFactory> dummy_factory_remote;
  auto dummy_factory_receiver =
      dummy_factory_remote.InitWithNewPipeAndPassReceiver();
  SetMediaFoundationInterfaceFactoryRemote(std::move(dummy_factory_remote));

  auto factory = GetMediaInterfaceFactory();
  ASSERT_TRUE(factory.is_bound());

  // Establish a hardware-secure CDM context first.
  factory->CreateCdm(CreateHwSecureCdmConfig(), base::DoNothing());
  base::RunLoop().RunUntilIdle();

  mojo::PendingRemote<media::mojom::MediaLog> media_log_remote;
  auto media_log_receiver = media_log_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<media::mojom::Renderer> renderer_remote;
  auto renderer_receiver = renderer_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<media::mojom::MediaFoundationRendererExtension>
      extension_remote;

  factory->CreateMediaFoundationRenderer(
      std::move(media_log_remote), std::move(renderer_receiver),
      extension_remote.InitWithNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  MediaPlayerId player_id(main_rfh()->GetGlobalId(), 1);
  EXPECT_TRUE(AudibilityBypassTracker::ClaimGrant(player_id));
}

TEST_F(MediaInterfaceProxyTest,
       CreateMediaFoundationRenderer_GrantWithClearPlayback) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      media::kMediaFoundationClearPlayback);

  mojo::PendingRemote<media::mojom::InterfaceFactory> dummy_factory_remote;
  auto dummy_factory_receiver =
      dummy_factory_remote.InitWithNewPipeAndPassReceiver();
  SetMediaFoundationInterfaceFactoryRemote(std::move(dummy_factory_remote));

  auto factory = GetMediaInterfaceFactory();
  ASSERT_TRUE(factory.is_bound());

  mojo::PendingRemote<media::mojom::MediaLog> media_log_remote;
  auto media_log_receiver = media_log_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<media::mojom::Renderer> renderer_remote;
  auto renderer_receiver = renderer_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<media::mojom::MediaFoundationRendererExtension>
      extension_remote;

  factory->CreateMediaFoundationRenderer(
      std::move(media_log_remote), std::move(renderer_receiver),
      extension_remote.InitWithNewPipeAndPassReceiver());
  base::RunLoop().RunUntilIdle();

  MediaPlayerId player_id(main_rfh()->GetGlobalId(), 1);
  EXPECT_TRUE(AudibilityBypassTracker::ClaimGrant(player_id));
}
#endif  // BUILDFLAG(IS_WIN)

}  // namespace content
