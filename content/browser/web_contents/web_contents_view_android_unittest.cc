// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include <memory>

#include "base/android/jni_string.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/gfx/image/image_skia.h"

namespace content {

namespace {

class MockWebContentsViewAndroid : public WebContentsViewAndroid {
 public:
  using WebContentsViewAndroid::WebContentsViewAndroid;

  void set_allowed(bool allowed) { allowed_ = allowed; }

  bool was_called() const { return was_called_; }
  bool system_drag_ended_called() const { return system_drag_ended_called_; }

  bool IsDragAllowedByDataControlPolicy(const ClipboardEndpoint& source,
                                        const DropData& drop_data) override {
    was_called_ = true;
    return allowed_;
  }

  void OnSystemDragEnded(RenderWidgetHost* source_rwh) override {
    system_drag_ended_called_ = true;
  }

 private:
  bool was_called_ = false;
  bool system_drag_ended_called_ = false;
  bool allowed_ = false;
};

class WebContentsViewAndroidTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    const char kGoogleUrl[] = "https://google.com/";
    NavigateAndCommit(GURL(kGoogleUrl));

    view_ = std::make_unique<MockWebContentsViewAndroid>(
        static_cast<WebContentsImpl*>(web_contents()), nullptr);
  }

  void TearDown() override {
    view_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  gfx::ImageSkia CreateValidDragImage() {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(1, 1);
    return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() {
    return static_cast<RenderWidgetHostImpl*>(
        web_contents()->GetPrimaryMainFrame()->GetRenderWidgetHost());
  }

  MockWebContentsViewAndroid* view() { return view_.get(); }

 private:
  std::unique_ptr<MockWebContentsViewAndroid> view_;
};

TEST_F(WebContentsViewAndroidTest, StartDragging_BlockedByPolicy) {
  view()->set_allowed(false);

  DropData drop_data;
  drop_data.text = u"Blocked Data";

  view()->StartDragging(*web_contents()->GetPrimaryMainFrame(), drop_data,
                        blink::kDragOperationCopy, CreateValidDragImage(),
                        gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo());

  EXPECT_TRUE(view()->was_called());
  EXPECT_TRUE(view()->system_drag_ended_called());
}

// TODO(b/507788269): Re-enable this test once fixed.
TEST_F(WebContentsViewAndroidTest, DISABLED_DropDataRestoredFromJava) {
  view()->set_allowed(true);

  // Simulate drop with custom data JSON and effectAllowed.
  std::vector<std::u16string> mime_types;
  JNIEnv* env = base::android::AttachCurrentThread();

  std::string custom_data_json = "{\"my-key\":\"my-value\"}";
  base::android::ScopedJavaLocalRef<jstring> j_custom_data =
      base::android::ConvertUTF8ToJavaString(env, custom_data_json);

  base::android::ScopedJavaLocalRef<jstring> j_effect_allowed =
      base::android::ConvertUTF8ToJavaString(env, "move");

  // Action 3 is ACTION_DROP.
  ui::DragEventAndroid drop_event(
      env, 3, gfx::PointF(), gfx::PointF(), mime_types,
      base::android::JavaRef<jstring>(), base::android::JavaRef<jobjectArray>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), j_custom_data, j_effect_allowed);

  view()->OnDragEvent(drop_event);

  // Verify that drop_data_ was populated from Java data.
  DropData* restored_data = view()->GetDropData();
  ASSERT_TRUE(restored_data);
  EXPECT_EQ(restored_data->custom_data[u"my-key"], u"my-value");
  EXPECT_EQ(restored_data->source_effect_allowed, u"move");
}

}  // namespace
}  // namespace content
