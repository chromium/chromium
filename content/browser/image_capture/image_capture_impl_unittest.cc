// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/image_capture/image_capture_impl.h"

#include "base/test/test_future.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/permission_controller.h"
#include "content/public/browser/permission_descriptor_util.h"
#include "content/public/test/permissions_test_utils.h"
#include "content/test/test_render_frame_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {
constexpr char kTestUrl[] = "https://google.com";
}  // namespace

class ImageCaptureImplTest : public RenderViewHostTestHarness {
 public:
 public:
  ImageCaptureImplTest() = default;
  ImageCaptureImplTest(const ImageCaptureImplTest&) = delete;
  ImageCaptureImplTest& operator=(const ImageCaptureImplTest&) = delete;
  ~ImageCaptureImplTest() override = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    origin_ = url::Origin::Create(GURL(kTestUrl));
    NavigateAndCommit(origin_.GetURL());
    auto receiver = image_capture_remote_.BindNewPipeAndPassReceiver();
    content::ImageCaptureImpl::Create(
        main_rfh(), std::move(receiver),
        /*skip_connecting_to_media_stream_manager_for_testing=*/true);
  }

  void HideView() { main_rfh()->GetView()->Hide(); }

  void SetPermissionForPTZ(blink::mojom::PermissionStatus status) {
    content::PermissionController* permission_controller =
        GetBrowserContext()->GetPermissionController();
    DCHECK(permission_controller);
    SetPermissionControllerOverride(permission_controller, origin_, origin_,
                                    blink::PermissionType::CAMERA_PAN_TILT_ZOOM,
                                    status);
  }

  void SetPhotoOptions(media::mojom::PhotoSettingsPtr photo_settings,
                       base::OnceCallback<void(bool)> callback) {
    CHECK_CURRENTLY_ON(BrowserThread::UI);
    image_capture_remote_->SetPhotoOptions("", std::move(photo_settings),
                                           std::move(callback));
  }

 private:
  mojo::Remote<media::mojom::ImageCapture> image_capture_remote_;
  url::Origin origin_;
};

TEST_F(ImageCaptureImplTest, SetPhotoOptionsDefault) {
  base::test::TestFuture<bool> future;
  SetPhotoOptions(media::mojom::PhotoSettings::New(), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

TEST_F(ImageCaptureImplTest, SetPhotoOptionsWithHiddenVisibility) {
  SetPermissionForPTZ(blink::mojom::PermissionStatus::GRANTED);

  HideView();

  base::test::TestFuture<bool> future;
  auto photo_settings = media::mojom::PhotoSettings::New();
  photo_settings->has_pan = true;
  photo_settings->has_tilt = true;
  photo_settings->has_zoom = true;
  SetPhotoOptions(std::move(photo_settings), future.GetCallback());
  EXPECT_FALSE(future.Get());

  future.Clear();
  SetPhotoOptions(media::mojom::PhotoSettings::New(), future.GetCallback());
  EXPECT_FALSE(future.Get());
}

// PTZ permission is always granted on Android (see
// MediaDevicesPermissionChecker::HasPanTiltZoomPermissionGrantedOnUIThread),
// overriding it will have no effect.
#if !BUILDFLAG(IS_ANDROID)
TEST_F(ImageCaptureImplTest, SetPhotoOptionsWithPTZNoPermission) {
  SetPermissionForPTZ(blink::mojom::PermissionStatus::DENIED);

  base::test::TestFuture<bool> future;
  auto photo_settings = media::mojom::PhotoSettings::New();
  photo_settings->has_pan = true;
  photo_settings->has_tilt = true;
  photo_settings->has_zoom = true;
  SetPhotoOptions(std::move(photo_settings), future.GetCallback());
  EXPECT_FALSE(future.Get());
}
#endif

TEST_F(ImageCaptureImplTest, SetPhotoOptionsWithPTZWithPermission) {
  SetPermissionForPTZ(blink::mojom::PermissionStatus::GRANTED);

  base::test::TestFuture<bool> future;
  auto photo_settings = media::mojom::PhotoSettings::New();
  photo_settings->has_pan = true;
  photo_settings->has_tilt = true;
  photo_settings->has_zoom = true;
  SetPhotoOptions(std::move(photo_settings), future.GetCallback());
  EXPECT_TRUE(future.Get());
}

}  // namespace content
