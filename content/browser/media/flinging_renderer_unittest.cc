// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/flinging_renderer.h"

#include <memory>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "base/version.h"
#include "content/browser/media/media_interface_proxy.h"
#include "content/browser/presentation/presentation_test_utils.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/media_controller.h"
#include "media/base/mock_filters.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "media/mojo/mojom/renderer.mojom.h"
#include "media/mojo/mojom/renderer_extensions.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::_;
using ::testing::StrictMock;
using ::testing::NiceMock;

namespace content {

class MockMediaController : public media::MediaController {
 public:
  MOCK_METHOD0(Play, void());
  MOCK_METHOD0(Pause, void());
  MOCK_METHOD1(SetMute, void(bool));
  MOCK_METHOD1(SetVolume, void(float));
  MOCK_METHOD1(Seek, void(base::TimeDelta));
};

class MockFlingingController : public media::FlingingController {
 public:
  explicit MockFlingingController(media::MediaController* media_controller)
      : media_controller_(media_controller) {}

  media::MediaController* GetMediaController() override {
    return media_controller_;
  }

  MOCK_METHOD1(AddMediaStatusObserver, void(media::MediaStatusObserver*));
  MOCK_METHOD1(RemoveMediaStatusObserver, void(media::MediaStatusObserver*));
  MOCK_METHOD0(GetApproximateCurrentTime, base::TimeDelta());

 private:
  raw_ptr<media::MediaController> media_controller_;
};

class FlingingRendererTest : public testing::Test {
 public:
  FlingingRendererTest()
      : media_controller_(new StrictMock<MockMediaController>()),
        flinging_controller_(
            new StrictMock<MockFlingingController>(media_controller_.get())) {
    EXPECT_CALL(*flinging_controller_, AddMediaStatusObserver(_));
    EXPECT_CALL(*flinging_controller_, RemoveMediaStatusObserver(_));

    renderer_ = base::WrapUnique(new FlingingRenderer(
        std::unique_ptr<media::FlingingController>(flinging_controller_),
        mojo::NullRemote()));

    renderer_->Initialize(nullptr, &renderer_client_, base::DoNothing());
  }

 protected:
  NiceMock<media::MockRendererClient> renderer_client_;
  std::unique_ptr<MockMediaController> media_controller_;
  std::unique_ptr<FlingingRenderer> renderer_;
  raw_ptr<StrictMock<MockFlingingController>> flinging_controller_;
};

TEST_F(FlingingRendererTest, StartPlayingFromTime) {
  base::TimeDelta seek_time = base::Seconds(10);
  EXPECT_CALL(*media_controller_, Seek(seek_time));

  renderer_->StartPlayingFrom(seek_time);
}

TEST_F(FlingingRendererTest, StartPlayingFromBeginning) {
  EXPECT_CALL(*media_controller_, Seek(base::TimeDelta()));

  renderer_->StartPlayingFrom(base::TimeDelta());
}

TEST_F(FlingingRendererTest, SetPlaybackRate) {
  double playback_rate = 1.0;
  EXPECT_CALL(*media_controller_, Play());

  renderer_->SetPlaybackRate(playback_rate);
}

TEST_F(FlingingRendererTest, SetPlaybackRateToZero) {
  double playback_rate = 0.0;
  EXPECT_CALL(*media_controller_, Pause());

  renderer_->SetPlaybackRate(playback_rate);
}

// Setting the volume to a positive value should not change the mute state.
TEST_F(FlingingRendererTest, SetVolume) {
  float volume = 0.5;
  EXPECT_CALL(*media_controller_, SetVolume(volume));
  EXPECT_CALL(*media_controller_, SetMute(_)).Times(0);

  renderer_->SetVolume(volume);
}

// Setting the volume to 0 should not set the mute state.
TEST_F(FlingingRendererTest, SetVolumeToZero) {
  float volume = 0;
  EXPECT_CALL(*media_controller_, SetVolume(volume));
  EXPECT_CALL(*media_controller_, SetMute(_)).Times(0);

  renderer_->SetVolume(volume);
}

namespace {

class ScopedFlingingController : public media::FlingingController,
                                 public media::MediaController {
 public:
  explicit ScopedFlingingController(bool* alive) : alive_(alive) {
    *alive_ = true;
  }
  ~ScopedFlingingController() override { *alive_ = false; }

  // media::FlingingController:
  media::MediaController* GetMediaController() override { return this; }
  void AddMediaStatusObserver(media::MediaStatusObserver*) override {}
  void RemoveMediaStatusObserver(media::MediaStatusObserver*) override {}
  base::TimeDelta GetApproximateCurrentTime() override {
    return base::TimeDelta();
  }

  // media::MediaController:
  void Play() override {}
  void Pause() override {}
  void SetMute(bool) override {}
  void SetVolume(float) override {}
  void Seek(base::TimeDelta) override {}

 private:
  raw_ptr<bool> alive_;
};

class StubFlingingRendererClientExtension
    : public media::mojom::FlingingRendererClientExtension {
 public:
  void OnRemotePlayStateChange(media::MediaStatus::State) override {}
};

class PresentationDelegateBrowserClient : public ContentBrowserClient {
 public:
  explicit PresentationDelegateBrowserClient(
      ControllerPresentationServiceDelegate* delegate)
      : delegate_(delegate) {}

  ControllerPresentationServiceDelegate*
  GetControllerPresentationServiceDelegate(WebContents*) override {
    return delegate_;
  }

 private:
  raw_ptr<ControllerPresentationServiceDelegate> delegate_;
};

}  // namespace

class FlingingRendererDocumentLifetimeTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    browser_client_ = std::make_unique<PresentationDelegateBrowserClient>(
        &presentation_delegate_);
    original_browser_client_ =
        SetBrowserClientForTesting(browser_client_.get());
  }

  void TearDown() override {
    SetBrowserClientForTesting(original_browser_client_);
    browser_client_.reset();
    RenderViewHostTestHarness::TearDown();
  }

 protected:
  NiceMock<MockPresentationServiceDelegate> presentation_delegate_;
  std::unique_ptr<PresentationDelegateBrowserClient> browser_client_;
  raw_ptr<ContentBrowserClient> original_browser_client_ = nullptr;
};

TEST_F(FlingingRendererDocumentLifetimeTest, DestroyedWithDocument) {
  constexpr char kPresentationId[] = "test-presentation-id";
  bool controller_alive = false;

  NavigateAndCommit(GURL("https://a.example/"));
  RenderFrameHost* rfh = main_rfh();
  EXPECT_CALL(presentation_delegate_,
              GetFlingingController(rfh->GetProcess()->GetDeprecatedID(),
                                    rfh->GetRoutingID(), kPresentationId))
      .WillOnce([&](int, int, const std::string&) {
        return std::make_unique<ScopedFlingingController>(&controller_alive);
      });

  StubFlingingRendererClientExtension client_extension_impl;
  mojo::Receiver<media::mojom::FlingingRendererClientExtension>
      client_extension_receiver(&client_extension_impl);
  mojo::Remote<media::mojom::InterfaceFactory> factory;
  static_cast<RenderFrameHostImpl*>(rfh)->BindMediaInterfaceFactoryReceiver(
      factory.BindNewPipeAndPassReceiver());

  mojo::Remote<media::mojom::Renderer> renderer;
  factory->CreateFlingingRenderer(
      kPresentationId, client_extension_receiver.BindNewPipeAndPassRemote(),
      renderer.BindNewPipeAndPassReceiver());
  factory.FlushForTesting();
  renderer.FlushForTesting();
  ASSERT_TRUE(renderer.is_connected());
  ASSERT_TRUE(controller_alive);

  NavigateAndCommit(GURL("https://b.example/"));
  web_contents()->GetController().GetBackForwardCache().Flush();
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !renderer.is_connected() && !controller_alive; }));
}

}  // namespace content
