// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/picture_in_picture/picture_in_picture_controller_impl.h"

#include <memory>

#include "media/mojo/mojom/media_player.mojom-blink.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_testing.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/events/event.h"
#include "third_party/blink/renderer/core/frame/frame_test_helpers.h"
#include "third_party/blink/renderer/core/frame/web_local_frame_impl.h"
#include "third_party/blink/renderer/core/html/media/html_media_test_helper.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/layout/layout_image.h"
#include "third_party/blink/renderer/core/loader/resource/image_resource_content.h"
#include "third_party/blink/renderer/core/testing/core_unit_test_helper.h"
#include "third_party/blink/renderer/core/testing/wait_for_event.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_component.h"
#include "third_party/blink/renderer/platform/mediastream/media_stream_descriptor.h"
#include "third_party/blink/renderer/platform/testing/empty_web_media_player.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/testing/url_test_helpers.h"

using ::testing::_;

namespace blink {

viz::SurfaceId TestSurfaceId() {
  // Use a fake but valid viz::SurfaceId.
  return {viz::FrameSinkId(1, 1),
          viz::LocalSurfaceId(
              11, base::UnguessableToken::Deserialize(0x111111, 0))};
}

// The MockPictureInPictureSession implements a PictureInPicture session in the
// same process as the test and guarantees that the callbacks are called in
// order for the events to be fired.
class MockPictureInPictureSession
    : public mojom::blink::PictureInPictureSession {
 public:
  MockPictureInPictureSession(
      mojo::PendingReceiver<mojom::blink::PictureInPictureSession> receiver)
      : receiver_(this, std::move(receiver)) {
    ON_CALL(*this, Stop(_)).WillByDefault([](StopCallback callback) {
      std::move(callback).Run();
    });
  }
  ~MockPictureInPictureSession() override = default;

  MOCK_METHOD(void, Stop, (StopCallback));
  MOCK_METHOD(void,
              Update,
              (uint32_t,
               mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
               const viz::SurfaceId&,
               const gfx::Size&,
               bool));

 private:
  mojo::Receiver<mojom::blink::PictureInPictureSession> receiver_;
};

// The MockPictureInPictureService implements the PictureInPicture service in
// the same process as the test and guarantees that the callbacks are called in
// order for the events to be fired.
class MockPictureInPictureService
    : public mojom::blink::PictureInPictureService {
 public:
  MockPictureInPictureService() {
    // Setup default implementations.
    ON_CALL(*this, StartSession(_, _, _, _, _, _, _, _))
        .WillByDefault(testing::Invoke(
            this, &MockPictureInPictureService::StartSessionInternal));
  }

  MockPictureInPictureService(const MockPictureInPictureService&) = delete;
  MockPictureInPictureService& operator=(const MockPictureInPictureService&) =
      delete;

  ~MockPictureInPictureService() override = default;

  void Bind(mojo::ScopedMessagePipeHandle handle) {
    receiver_.Bind(mojo::PendingReceiver<mojom::blink::PictureInPictureService>(
        std::move(handle)));

    session_ = std::make_unique<MockPictureInPictureSession>(
        session_remote_.InitWithNewPipeAndPassReceiver());
  }

  MOCK_METHOD(
      void,
      StartSession,
      (uint32_t,
       mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
       const viz::SurfaceId&,
       const gfx::Size&,
       bool,
       mojo::PendingRemote<mojom::blink::PictureInPictureSessionObserver>,
       const gfx::Rect&,
       StartSessionCallback));

  MockPictureInPictureSession& Session() { return *session_.get(); }

  void StartSessionInternal(
      uint32_t,
      mojo::PendingAssociatedRemote<media::mojom::blink::MediaPlayer>,
      const viz::SurfaceId&,
      const gfx::Size&,
      bool,
      mojo::PendingRemote<mojom::blink::PictureInPictureSessionObserver>,
      const gfx::Rect& source_bounds,
      StartSessionCallback callback) {
    source_bounds_ = source_bounds;
    std::move(callback).Run(std::move(session_remote_), gfx::Size());
  }

  const gfx::Rect& source_bounds() const { return source_bounds_; }

 private:
  mojo::Receiver<mojom::blink::PictureInPictureService> receiver_{this};
  std::unique_ptr<MockPictureInPictureSession> session_;
  mojo::PendingRemote<mojom::blink::PictureInPictureSession> session_remote_;
  gfx::Rect source_bounds_;
};

class PictureInPictureControllerFrameClient
    : public test::MediaStubLocalFrameClient {
 public:
  static PictureInPictureControllerFrameClient* Create(
      std::unique_ptr<WebMediaPlayer> player) {
    return MakeGarbageCollected<PictureInPictureControllerFrameClient>(
        std::move(player));
  }

  explicit PictureInPictureControllerFrameClient(
      std::unique_ptr<WebMediaPlayer> player)
      : test::MediaStubLocalFrameClient(std::move(player)) {}

  PictureInPictureControllerFrameClient(
      const PictureInPictureControllerFrameClient&) = delete;
  PictureInPictureControllerFrameClient& operator=(
      const PictureInPictureControllerFrameClient&) = delete;
};

class PictureInPictureControllerPlayer final : public EmptyWebMediaPlayer {
 public:
  PictureInPictureControllerPlayer() = default;

  PictureInPictureControllerPlayer(const PictureInPictureControllerPlayer&) =
      delete;
  PictureInPictureControllerPlayer& operator=(
      const PictureInPictureControllerPlayer&) = delete;

  ~PictureInPictureControllerPlayer() override = default;

  double Duration() const override {
    if (infinity_duration_)
      return std::numeric_limits<double>::infinity();
    return EmptyWebMediaPlayer::Duration();
  }
  ReadyState GetReadyState() const override { return kReadyStateHaveMetadata; }
  bool HasVideo() const override { return true; }
  void OnRequestPictureInPicture() override { surface_id_ = TestSurfaceId(); }
  absl::optional<viz::SurfaceId> GetSurfaceId() override { return surface_id_; }

  void set_infinity_duration(bool value) { infinity_duration_ = value; }

 private:
  bool infinity_duration_ = false;
  absl::optional<viz::SurfaceId> surface_id_;
};

class PictureInPictureTestWebFrameClient
    : public frame_test_helpers::TestWebFrameClient {
 public:
  explicit PictureInPictureTestWebFrameClient(
      std::unique_ptr<WebMediaPlayer> web_media_player)
      : web_media_player_(std::move(web_media_player)) {}

  WebMediaPlayer* CreateMediaPlayer(
      const WebMediaPlayerSource&,
      WebMediaPlayerClient*,
      blink::MediaInspectorContext*,
      WebMediaPlayerEncryptedMediaClient*,
      WebContentDecryptionModule*,
      const WebString& sink_id,
      const cc::LayerTreeSettings& settings,
      scoped_refptr<base::TaskRunner> compositor_worker_task_runner) override {
    return web_media_player_.release();
  }

 private:
  std::unique_ptr<WebMediaPlayer> web_media_player_;
};

class PictureInPictureControllerTest : public RenderingTest {
 public:
  void SetUp() override {
    client_ = std::make_unique<PictureInPictureTestWebFrameClient>(
        std::make_unique<PictureInPictureControllerPlayer>());

    helper_.Initialize(client_.get());

    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_,
        WTF::BindRepeating(&MockPictureInPictureService::Bind,
                           WTF::Unretained(&mock_service_)));

    video_ = MakeGarbageCollected<HTMLVideoElement>(GetDocument());
    GetDocument().body()->AppendChild(video_);
    Video()->SetReadyState(HTMLMediaElement::ReadyState::kHaveMetadata);
    layer_ = cc::Layer::Create();
    Video()->SetCcLayerForTesting(layer_.get());

    std::string test_name =
        testing::UnitTest::GetInstance()->current_test_info()->name();
    if (test_name.find("MediaSource") != std::string::npos) {
      MediaStreamComponentVector dummy_tracks;
      auto* descriptor = MakeGarbageCollected<MediaStreamDescriptor>(
          dummy_tracks, dummy_tracks);
      Video()->SetSrcObjectVariant(descriptor);
    } else {
      Video()->SetSrc("http://example.com/foo.mp4");
    }

    test::RunPendingTasks();
  }

  void TearDown() override {
    GetFrame().GetBrowserInterfaceBroker().SetBinderForTesting(
        mojom::blink::PictureInPictureService::Name_, {});
  }

  HTMLVideoElement* Video() const { return video_.Get(); }
  MockPictureInPictureService& Service() { return mock_service_; }

  LocalFrame& GetFrame() const { return *helper_.LocalMainFrame()->GetFrame(); }

  Document& GetDocument() const { return *GetFrame().GetDocument(); }

  WebFrameWidgetImpl* GetWidget() const {
    return static_cast<WebFrameWidgetImpl*>(
        GetDocument().GetFrame()->GetWidgetForLocalRoot());
  }

  void ResetMediaPlayerAndMediaSource() {
    DynamicTo<HTMLMediaElement>(Video())->ResetMediaPlayerAndMediaSource();
  }

 private:
  Persistent<HTMLVideoElement> video_;
  std::unique_ptr<frame_test_helpers::TestWebFrameClient> client_;
  testing::NiceMock<MockPictureInPictureService> mock_service_;
  scoped_refptr<cc::Layer> layer_;
  frame_test_helpers::WebViewHelper helper_;
};

TEST_F(PictureInPictureControllerTest, EnterPictureInPictureFiresEvent) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  EXPECT_NE(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());
}

TEST_F(PictureInPictureControllerTest,
       FrameThrottlingIsSetProperlyWithoutSetup) {
  // This test assumes that it throttling is allowed by default.
  ASSERT_TRUE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());

  // Entering PictureInPicture should disallow throttling.
  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);
  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);
  EXPECT_FALSE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());

  // Exiting PictureInPicture should re-enable it.
  PictureInPictureControllerImpl::From(GetDocument())
      .ExitPictureInPicture(Video(), nullptr /* resolver */);
  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kLeavepictureinpicture);
  EXPECT_TRUE(GetWidget()->GetMayThrottleIfUndrawnFramesForTesting());
}

TEST_F(PictureInPictureControllerTest, ExitPictureInPictureFiresEvent) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  EXPECT_CALL(Service().Session(), Stop(_));

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  PictureInPictureControllerImpl::From(GetDocument())
      .ExitPictureInPicture(Video(), nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kLeavepictureinpicture);

  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());
}

TEST_F(PictureInPictureControllerTest, StartObserving) {
  EXPECT_FALSE(PictureInPictureControllerImpl::From(GetDocument())
                   .IsSessionObserverReceiverBoundForTesting());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  EXPECT_TRUE(PictureInPictureControllerImpl::From(GetDocument())
                  .IsSessionObserverReceiverBoundForTesting());
}

TEST_F(PictureInPictureControllerTest, StopObserving) {
  EXPECT_FALSE(PictureInPictureControllerImpl::From(GetDocument())
                   .IsSessionObserverReceiverBoundForTesting());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  EXPECT_CALL(Service().Session(), Stop(_));

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  PictureInPictureControllerImpl::From(GetDocument())
      .ExitPictureInPicture(Video(), nullptr);
  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kLeavepictureinpicture);

  EXPECT_FALSE(PictureInPictureControllerImpl::From(GetDocument())
                   .IsSessionObserverReceiverBoundForTesting());
}

TEST_F(PictureInPictureControllerTest, PlayPauseButton_InfiniteDuration) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  Video()->DurationChanged(std::numeric_limits<double>::infinity(), false);

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), false, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);
}

TEST_F(PictureInPictureControllerTest, PlayPauseButton_MediaSource) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  // The test automatically setup the WebMediaPlayer with a MediaSource based on
  // the test name.

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), false, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);
}

TEST_F(PictureInPictureControllerTest, PerformMediaPlayerAction) {
  frame_test_helpers::WebViewHelper helper;
  helper.Initialize();

  WebLocalFrameImpl* frame = helper.LocalMainFrame();
  Document* document = frame->GetFrame()->GetDocument();

  Persistent<HTMLVideoElement> video =
      MakeGarbageCollected<HTMLVideoElement>(*document);
  document->body()->AppendChild(video);

  gfx::Point bounds = video->BoundsInViewport().CenterPoint();

  // Performs the specified media player action on the media element at the
  // given location.
  frame->GetFrame()->MediaPlayerActionAtViewportPoint(
      bounds, blink::mojom::MediaPlayerActionType::kPictureInPicture, true);
}

TEST_F(PictureInPictureControllerTest, EnterPictureInPictureAfterResettingWMP) {
  V8TestingScope scope;

  EXPECT_NE(nullptr, Video()->GetWebMediaPlayer());

  // Reset web media player.
  ResetMediaPlayerAndMediaSource();
  EXPECT_EQ(nullptr, Video()->GetWebMediaPlayer());

  auto* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(scope.GetScriptState());
  auto promise = resolver->Promise();
  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), resolver);

  // Verify rejected with DOMExceptionCode::kInvalidStateError.
  EXPECT_EQ(v8::Promise::kRejected, promise.V8Promise()->State());
  DOMException* dom_exception = V8DOMException::ToImplWithTypeCheck(
      promise.GetIsolate(), promise.V8Promise()->Result());
  ASSERT_NE(dom_exception, nullptr);
  EXPECT_EQ(static_cast<int>(DOMExceptionCode::kInvalidStateError),
            dom_exception->code());
}

TEST_F(PictureInPictureControllerTest,
       EnterPictureInPictureProvideSourceBoundsSetToBoundsInViewport) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  // We expect that the video element has some nontrivial rect, else this won't
  // really test anything.
  ASSERT_NE(Video()->BoundsInViewport(), gfx::Rect());
  EXPECT_EQ(Service().source_bounds(), Video()->BoundsInViewport());
}

TEST_F(PictureInPictureControllerTest,
       EnterPictureInPictureProvideSourceBoundsSetToReplacedContentRect) {
  // Create one image with a size of 10x10px
  SkImageInfo raster_image_info =
      SkImageInfo::MakeN32Premul(10, 10, SkColorSpace::MakeSRGB());
  sk_sp<SkSurface> surface(SkSurface::MakeRaster(raster_image_info));
  ImageResourceContent* image_content = ImageResourceContent::CreateLoaded(
      UnacceleratedStaticBitmapImage::Create(surface->makeImageSnapshot())
          .get());

  Element* div = GetDocument().CreateRawElement(html_names::kDivTag);
  div->setAttribute(html_names::kStyleAttr,
                    "padding: 100px;"
                    "width: 150px;"
                    "height: 150px;"
                    "padding: 100px;"
                    "transform: scale(2)");
  GetDocument().body()->AppendChild(div);
  div->AppendChild(Video());
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  // Set poster image to video
  auto* layout_image = To<LayoutImage>(Video()->GetLayoutObject());
  const char kPosterUrl[] = "http://example.com/foo.jpg";
  url_test_helpers::RegisterMockedErrorURLLoad(
      url_test_helpers::ToKURL(kPosterUrl));
  Video()->setAttribute(html_names::kPosterAttr, kPosterUrl);
  Video()->setAttribute(html_names::kStyleAttr,
                        "object-fit: none;"
                        "height: 150px;"
                        "width: 150px;");
  layout_image->ImageResource()->SetImageResource(image_content);
  GetDocument().View()->UpdateAllLifecyclePhasesForTest();

  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .PictureInPictureElement());

  WebMediaPlayer* player = Video()->GetWebMediaPlayer();
  EXPECT_CALL(Service(),
              StartSession(player->GetDelegateId(), _, TestSurfaceId(),
                           player->NaturalSize(), true, _, _, _));

  PictureInPictureControllerImpl::From(GetDocument())
      .EnterPictureInPicture(Video(), /*promise=*/nullptr);

  MakeGarbageCollected<WaitForEvent>(Video(),
                                     event_type_names::kEnterpictureinpicture);

  // Source bounds are expected to match the poster image size, not the bounds
  // of the video element.
  EXPECT_EQ(Video()->BoundsInViewport(), gfx::Rect(33, 33, 300, 300));
  EXPECT_EQ(Service().source_bounds(), gfx::Rect(173, 173, 20, 20));
}

TEST_F(PictureInPictureControllerTest, VideoIsNotAllowedIfAutoPip) {
  EXPECT_EQ(PictureInPictureControllerImpl::Status::kEnabled,
            PictureInPictureControllerImpl::From(GetDocument())
                .IsElementAllowed(*Video(), /*report_failure=*/false));

  // Simulate auto-pip mode.
  Video()->SetPersistentState(true);

  EXPECT_EQ(PictureInPictureControllerImpl::Status::kAutoPipAndroid,
            PictureInPictureControllerImpl::From(GetDocument())
                .IsElementAllowed(*Video(), /*report_failure=*/false));
}

TEST_F(PictureInPictureControllerTest, CreateDocumentPictureInPictureWindow) {
  EXPECT_EQ(nullptr, PictureInPictureControllerImpl::From(GetDocument())
                         .pictureInPictureWindow());

  // Enable the DocumentPictureInPictureAPI flag.
  ScopedPictureInPictureAPIForTest scoped_dependency(true);
  ScopedDocumentPictureInPictureAPIForTest scoped_feature(true);

  V8TestingScope scope;
  KURL url = KURL("https://example.com/");

  // Get pass the LocalDOMWindow::isSecureContext() check.
  GetFrame().DomWindow()->GetSecurityContext().SetSecurityOriginForTesting(
      nullptr);
  GetFrame().DomWindow()->GetSecurityContext().SetSecurityOrigin(
      SecurityOrigin::Create(url));

  // Get pass the BindingSecurity::ShouldAllowAccessTo() check.
  ScriptState* script_state =
      ToScriptStateForMainWorld(GetDocument().GetFrame());
  ScriptState::Scope entered_context_scope(script_state);
  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);

  // Create the PictureInPictureWindowOptions.
  v8::Local<v8::Object> v8_object = v8::Object::New(scope.GetIsolate());
  v8_object
      ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "width"),
            v8::Number::New(scope.GetIsolate(), 640))
      .Check();
  v8_object
      ->Set(scope.GetContext(), V8String(scope.GetIsolate(), "height"),
            v8::Number::New(scope.GetIsolate(), 320))
      .Check();
  PictureInPictureWindowOptions* options =
      PictureInPictureWindowOptions::Create(resolver->Promise().GetIsolate(),
                                            v8_object,
                                            scope.GetExceptionState());

  // Set a base URL for the opener window.
  GetDocument().SetBaseURLOverride(url);
  EXPECT_EQ(url.GetString(), GetDocument().BaseURL().GetString());

  PictureInPictureControllerImpl::From(GetDocument())
      .CreateDocumentPictureInPictureWindow(
          script_state, *GetFrame().DomWindow(), options, resolver,
          scope.GetExceptionState());

  PictureInPictureWindow* pictureInPictureWindow =
      PictureInPictureControllerImpl::From(GetDocument())
          .pictureInPictureWindow();
  EXPECT_NE(nullptr, pictureInPictureWindow);
  EXPECT_EQ(url.GetString(),
            pictureInPictureWindow->document()->BaseURL().GetString());
}

}  // namespace blink
