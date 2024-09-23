// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture_handle_manager.h"

#include "content/test/test_render_view_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

using Callback = CaptureHandleManager::DeviceCaptureHandleChangeCallback;
using blink::MediaStreamDevice;
using testing::_;

const std::string kLabel = "noladaleybeceipretsamrehtonatey";

MATCHER(IsNullCaptureHandle, "") {
  static_assert(
      std::is_same<decltype(arg), const media::mojom::CaptureHandlePtr&>::value,
      "Matcher applied to incorrect type.");

  return !arg;
}

MATCHER_P2(IsCaptureHandle, expected_origin, expected_handle, "") {
  if (!arg) {
    return false;
  }

  if (arg->origin.opaque() != expected_origin.opaque()) {
    return false;  // One is empty, the other is non-empty.
  }

  // Either both are opaque or neither is. We only compare non-opaque origins.
  if (!expected_origin.opaque()) {
    if (arg->origin != expected_origin) {
      return false;
    }
  }

  return arg->capture_handle == expected_handle;
}

media::mojom::CaptureHandlePtr MakeCaptureHandle(url::Origin origin,
                                                 const std::u16string& handle) {
  auto capture_handle = media::mojom::CaptureHandle::New();
  capture_handle->origin = origin;
  capture_handle->capture_handle = handle;
  return capture_handle;
}

media::mojom::CaptureHandlePtr MakeCaptureHandle(const std::u16string& handle) {
  return MakeCaptureHandle(url::Origin(), handle);
}

blink::mojom::CaptureHandleConfigPtr MakePermissiveConfigWithHandle(
    const std::u16string& handle = u"") {
  auto ptr = blink::mojom::CaptureHandleConfig::New();
  ptr->expose_origin = true;
  ptr->capture_handle = handle;
  ptr->all_origins_permitted = true;
  return ptr;
}

blink::mojom::CaptureHandleConfigPtr MakeRestrictiveConfigWithHandle(
    const std::vector<url::Origin> permitted_origins,
    const std::u16string& handle = u"") {
  auto ptr = blink::mojom::CaptureHandleConfig::New();
  ptr->expose_origin = true;
  ptr->capture_handle = handle;
  ptr->all_origins_permitted = false;
  ptr->permitted_origins = permitted_origins;
  return ptr;
}

class CallbackHelper {
 public:
  MOCK_METHOD3(Method,
               void(const std::string& label,
                    blink::mojom::MediaStreamType type,
                    media::mojom::CaptureHandlePtr capture_handle));

  Callback AsCallback() {
    return base::BindRepeating(&CallbackHelper::Method, base::Unretained(this));
  }
};

}  // namespace

class CaptureHandleManagerTest : public RenderViewHostImplTestHarness {
 public:
  std::unique_ptr<TestWebContents> MakeTestWebContents() {
    scoped_refptr<SiteInstance> instance =
        SiteInstance::Create(GetBrowserContext());
    instance->GetProcess()->Init();

    return TestWebContents::Create(GetBrowserContext(), std::move(instance));
  }

  CallbackHelper& MakeCallbackHelper() {
    CHECK_LT(callback_helper_count_, kMaxCallbackHelpers);
    return callback_helpers_[++callback_helper_count_];
  }

  MediaStreamDevice& MakeDevice(
      RenderFrameHost* frame,
      media::mojom::CaptureHandlePtr capture_handle = nullptr) {
    CHECK_LT(device_count_, kMaxDevices);
    auto& device = devices_[++device_count_];

    const WebContentsMediaCaptureId id(frame->GetProcess()->GetID(),
                                       frame->GetRoutingID());
    device.id = id.ToString();
    device.display_media_info = media::mojom::DisplayMediaInformation::New();
    device.display_media_info->capture_handle = std::move(capture_handle);

    return device;
  }

  MediaStreamDevice& MakeDevice(
      std::unique_ptr<TestWebContents>& web_contents,
      media::mojom::CaptureHandlePtr capture_handle = nullptr) {
    return MakeDevice(web_contents->GetPrimaryMainFrame(),
                      std::move(capture_handle));
  }

 protected:
  CaptureHandleManager manager_;

 private:
  static constexpr size_t kMaxCallbackHelpers = 10;
  std::array<CallbackHelper, kMaxCallbackHelpers> callback_helpers_;
  size_t callback_helper_count_ = 0;

  static constexpr size_t kMaxDevices = 10;
  std::array<MediaStreamDevice, kMaxDevices> devices_;
  size_t device_count_ = 0;
};

TEST_F(CaptureHandleManagerTest,
       OnTabCaptureStartedProducesNoCallbackIfDeviceHasNoCaptureHandle) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  auto& callback_helper = MakeCallbackHelper();
  EXPECT_CALL(callback_helper, Method(_, _, _)).Times(0);
  manager_.OnTabCaptureStarted(kLabel, MakeDevice(captured),
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());
}

TEST_F(CaptureHandleManagerTest,
       OnTabCaptureStartedProducesCallbackIfDeviceHasOldCaptureHandle) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  auto& captured_device = MakeDevice(captured, MakeCaptureHandle(u"same"));

  captured->SetCaptureHandleConfig(MakePermissiveConfigWithHandle(u"same"));

  auto& callback_helper = MakeCallbackHelper();
  EXPECT_CALL(callback_helper, Method(_, _, _)).Times(0);
  manager_.OnTabCaptureStarted(kLabel, captured_device,
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());
}

TEST_F(CaptureHandleManagerTest,
       OnTabCaptureStartedProducesCallbackIfDeviceHasNewCaptureHandle) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  auto& captured_device = MakeDevice(captured, MakeCaptureHandle(u"old"));

  captured->SetCaptureHandleConfig(MakePermissiveConfigWithHandle(u"new"));

  auto& callback_helper = MakeCallbackHelper();
  EXPECT_CALL(callback_helper, Method(kLabel, captured_device.type,
                                      IsCaptureHandle(url::Origin(), u"new")))
      .Times(1);
  manager_.OnTabCaptureStarted(kLabel, captured_device,
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());
}

TEST_F(CaptureHandleManagerTest, CallbackInvokedWhenCaptureHandleChanges) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  auto& captured_device = MakeDevice(captured, MakeCaptureHandle(u"before"));
  captured->SetCaptureHandleConfig(MakePermissiveConfigWithHandle(u"before"));

  auto& callback_helper = MakeCallbackHelper();
  manager_.OnTabCaptureStarted(kLabel, captured_device,
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());

  EXPECT_CALL(callback_helper, Method(kLabel, captured_device.type,
                                      IsCaptureHandle(url::Origin(), u"after")))
      .Times(1);
  captured->SetCaptureHandleConfig(MakePermissiveConfigWithHandle(u"after"));
}

TEST_F(CaptureHandleManagerTest, CaptureHandleResetByNavigation) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  const GURL kGurl1("https://origin1.com");
  const GURL kGurl2("https://origin2.com");
  const url::Origin kOrigin1 = url::Origin::Create(kGurl1);

  captured->NavigateAndCommit(kGurl1);
  auto& captured_device =
      MakeDevice(captured, MakeCaptureHandle(kOrigin1, u"handle"));
  captured->SetCaptureHandleConfig(MakePermissiveConfigWithHandle(u"handle"));

  auto& callback_helper = MakeCallbackHelper();
  manager_.OnTabCaptureStarted(kLabel, captured_device,
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());

  EXPECT_CALL(callback_helper,
              Method(kLabel, captured_device.type, IsNullCaptureHandle()))
      .Times(1);
  captured->NavigateAndCommit(kGurl2);
}

TEST_F(CaptureHandleManagerTest,
       CallbackNotInvokedWhenConfigDisallowsCapturer) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  const GURL kCapturedUrl("https://captured.com");
  captured->NavigateAndCommit(kCapturedUrl);

  const GURL kDisallowedUrl("https://disallowed.com");
  capturer->NavigateAndCommit(kDisallowedUrl);

  const GURL kAllowedUrl("https://allowed.com");
  const url::Origin kAllowedOrigin = url::Origin::Create(kAllowedUrl);

  auto& captured_device = MakeDevice(captured);

  auto& callback_helper = MakeCallbackHelper();
  manager_.OnTabCaptureStarted(kLabel, captured_device,
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());

  EXPECT_CALL(callback_helper, Method(_, _, _)).Times(0);
  captured->SetCaptureHandleConfig(
      MakeRestrictiveConfigWithHandle({kAllowedOrigin}, u"handle"));
}

TEST_F(CaptureHandleManagerTest, CallbackInvokedWhenConfigAllowsCapturer) {
  auto captured = MakeTestWebContents();
  auto capturer = MakeTestWebContents();

  const GURL kCapturedUrl("https://captured.com");
  const url::Origin kCapturedOrigin = url::Origin::Create(kCapturedUrl);
  captured->NavigateAndCommit(kCapturedUrl);

  const GURL kAllowedUrl("https://allowed.com");
  const url::Origin kAllowedOrigin = url::Origin::Create(kAllowedUrl);
  capturer->NavigateAndCommit(kAllowedUrl);

  auto& captured_device = MakeDevice(captured);

  auto& callback_helper = MakeCallbackHelper();
  manager_.OnTabCaptureStarted(kLabel, captured_device,
                               capturer->GetPrimaryMainFrame()->GetGlobalId(),
                               callback_helper.AsCallback());

  EXPECT_CALL(callback_helper,
              Method(kLabel, captured_device.type,
                     IsCaptureHandle(kCapturedOrigin, u"handle")))
      .Times(1);
  captured->SetCaptureHandleConfig(
      MakeRestrictiveConfigWithHandle({kAllowedOrigin}, u"handle"));
}

}  // namespace content
