// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include <memory>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "content/browser/android/drop_data_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/common/drop_data.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/window_android.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/color/color_provider.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/gfx/image/image_skia.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/jar_jni/DragEvent_jni.h"

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

}  // namespace

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

TEST_F(WebContentsViewAndroidTest, DropDataRestoredFromJava) {
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
      env, 3, gfx::PointF(), gfx::PointF(), mime_types, false,
      base::android::JavaRef<jobjectArray>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      j_custom_data, j_effect_allowed);

  view()->OnDragEvent(drop_event);

  // Verify that drop_data_ was populated from Java data.
  DropData* restored_data = view()->GetDropData();
  ASSERT_TRUE(restored_data);
  EXPECT_EQ(restored_data->custom_data[u"my-key"], u"my-value");
  EXPECT_EQ(restored_data->source_effect_allowed, u"move");
}

TEST_F(WebContentsViewAndroidTest, DragInaccessibleImage_SameTab_Filtered) {
  DropData drop_data;
  drop_data.file_contents = {'t', 'e', 's', 't'};
  drop_data.file_contents_image_accessible = false;
  drop_data.file_contents_source_url =
      GURL("https://different-origin.com/image.png");
  drop_data.file_contents_filename_extension = "png";
  view()->drag_security_info_.OnDragInitiated(GetRenderWidgetHost(), drop_data);

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with image MIME type.
  std::vector<std::u16string> enter_mime_types = {u"image/png"};
  ui::DragEventAndroid enter_event(
      env, DragEventJni::ACTION_DRAG_ENTERED, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, base::android::JavaRef<jobjectArray>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>());
  view()->OnDragEvent(enter_event);

  // File metadata should not be advertised when image access is not allowed.
  EXPECT_TRUE(view()->drag_metadata_.empty());

  // ACTION_DROP with filenames.
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://org.chromium.test/image.png", "image.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event(
      env, DragEventJni::ACTION_DROP, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, j_filenames, base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>());
  view()->OnDragEvent(drop_event);

  DropData* restored_data = view()->GetDropData();
  ASSERT_TRUE(restored_data);
  EXPECT_TRUE(restored_data->filenames.empty());
  EXPECT_TRUE(restored_data->file_contents.empty());
  EXPECT_FALSE(restored_data->file_contents_image_accessible);
}

TEST_F(WebContentsViewAndroidTest, DragAccessibleImage_SameTab_Allowed) {
  DropData drop_data;
  drop_data.file_contents = {'t', 'e', 's', 't'};
  drop_data.file_contents_image_accessible = true;
  drop_data.file_contents_source_url = GURL("https://google.com/image.png");
  drop_data.file_contents_filename_extension = "png";
  view()->drag_security_info_.OnDragInitiated(GetRenderWidgetHost(), drop_data);

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with image MIME type.
  std::vector<std::u16string> enter_mime_types = {u"image/png"};
  ui::DragEventAndroid enter_event(
      env, DragEventJni::ACTION_DRAG_ENTERED, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, base::android::JavaRef<jobjectArray>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>());
  view()->OnDragEvent(enter_event);

  // File metadata should be advertised when image access is allowed.
  ASSERT_EQ(view()->drag_metadata_.size(), 1u);
  EXPECT_EQ(view()->drag_metadata_[0].kind, DropData::Kind::FILENAME);
  EXPECT_EQ(view()->drag_metadata_[0].filename, base::FilePath("file.png"));

  // ACTION_DROP with filenames.
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://org.chromium.test/image.png", "image.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event(
      env, DragEventJni::ACTION_DROP, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, j_filenames, base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>());
  view()->OnDragEvent(drop_event);

  DropData* restored_data = view()->GetDropData();
  ASSERT_TRUE(restored_data);
  ASSERT_EQ(restored_data->filenames.size(), 1u);
  EXPECT_EQ(restored_data->filenames[0].path,
            base::FilePath("content://org.chromium.test/image.png"));
  EXPECT_EQ(restored_data->filenames[0].display_name,
            base::FilePath("image.png"));
}

TEST_F(WebContentsViewAndroidTest, DragImage_ExternalSource_Allowed) {
  // No drag initiated on view.
  EXPECT_FALSE(view()->drag_security_info_.did_initiate());

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with image MIME type.
  std::vector<std::u16string> enter_mime_types = {u"image/png"};
  ui::DragEventAndroid enter_event(
      env, DragEventJni::ACTION_DRAG_ENTERED, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, base::android::JavaRef<jobjectArray>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>());
  view()->OnDragEvent(enter_event);

  ASSERT_EQ(view()->drag_metadata_.size(), 1u);
  EXPECT_EQ(view()->drag_metadata_[0].kind, DropData::Kind::FILENAME);

  // ACTION_DROP with filenames.
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://external.app/photo.png", "photo.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event(
      env, DragEventJni::ACTION_DROP, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, j_filenames, base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>());
  view()->OnDragEvent(drop_event);

  DropData* restored_data = view()->GetDropData();
  ASSERT_TRUE(restored_data);
  ASSERT_EQ(restored_data->filenames.size(), 1u);
  EXPECT_EQ(restored_data->filenames[0].path,
            base::FilePath("content://external.app/photo.png"));
}

TEST_F(WebContentsViewAndroidTest,
       DragInaccessibleImage_MixedMimeTypes_PreservesStringTypes) {
  DropData drop_data;
  drop_data.file_contents = {'t', 'e', 's', 't'};
  drop_data.file_contents_image_accessible = false;
  drop_data.file_contents_source_url =
      GURL("https://different-origin.com/image.png");
  drop_data.file_contents_filename_extension = "png";
  view()->drag_security_info_.OnDragInitiated(GetRenderWidgetHost(), drop_data);

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with text and image MIME types.
  std::vector<std::u16string> enter_mime_types = {ui::kMimeTypePlainText16,
                                                  u"image/png"};
  ui::DragEventAndroid enter_event(
      env, DragEventJni::ACTION_DRAG_ENTERED, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, base::android::JavaRef<jobjectArray>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>());
  view()->OnDragEvent(enter_event);

  // String MIME type is kept in metadata, while file metadata is omitted.
  ASSERT_EQ(view()->drag_metadata_.size(), 1u);
  EXPECT_EQ(view()->drag_metadata_[0].kind, DropData::Kind::STRING);
  EXPECT_EQ(view()->drag_metadata_[0].mime_type, ui::kMimeTypePlainText16);

  // ACTION_DROP with text and filenames.
  base::android::ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF8ToJavaString(env, "sample text");
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://org.chromium.test/image.png", "image.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event(
      env, DragEventJni::ACTION_DROP, gfx::PointF(), gfx::PointF(),
      enter_mime_types, false, j_filenames, j_text,
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>());
  view()->OnDragEvent(drop_event);

  DropData* restored_data = view()->GetDropData();
  ASSERT_TRUE(restored_data);
  EXPECT_EQ(restored_data->text, u"sample text");
  EXPECT_TRUE(restored_data->filenames.empty());
}

TEST_F(WebContentsViewAndroidTest, OnDragEnded_ResetsDropDataAndSecurityInfo) {
  DropData drop_data;
  drop_data.file_contents = {'t', 'e', 's', 't'};
  drop_data.file_contents_image_accessible = false;
  view()->drag_security_info_.OnDragInitiated(GetRenderWidgetHost(), drop_data);

  EXPECT_TRUE(view()->drag_security_info_.did_initiate());
  EXPECT_FALSE(view()->drag_security_info_.IsImageAccessibleFromFrame());

  JNIEnv* env = base::android::AttachCurrentThread();
  ui::DragEventAndroid end_event(
      env, DragEventJni::ACTION_DRAG_ENDED, gfx::PointF(), gfx::PointF(), {},
      false, base::android::JavaRef<jobjectArray>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>(), base::android::JavaRef<jstring>(),
      base::android::JavaRef<jstring>());

  view()->OnDragEvent(end_event);

  EXPECT_FALSE(view()->drag_security_info_.did_initiate());
  EXPECT_TRUE(view()->drag_security_info_.IsImageAccessibleFromFrame());
}

TEST_F(WebContentsViewAndroidTest, ColorProviderSourceFallback) {
  WebContentsImpl* web_contents_impl =
      static_cast<WebContentsImpl*>(web_contents());

  // Create a WindowAndroid for testing.
  std::unique_ptr<ui::WindowAndroid::ScopedWindowAndroidForTesting> window =
      ui::WindowAndroid::CreateForTesting();
  ui::WindowAndroid* window_android = window->get();

  // 1. Initial State: No window attached. The source should be the default
  // source (non-null).
  const ui::ColorProviderSource* default_source =
      web_contents_impl->GetColorProviderSourceForTesting();
  EXPECT_NE(default_source, nullptr);
  EXPECT_NE(default_source, window_android);
  web_contents()->GetColorProvider();

  // 2. Attach a WindowAndroid.
  web_contents_impl->SetColorProviderSource(window_android);
  EXPECT_EQ(web_contents_impl->GetColorProviderSourceForTesting(),
            window_android);
  web_contents()->GetColorProvider();

  // 3. Detach window. Should fall back to the default source synchronously.
  web_contents_impl->SetColorProviderSource(nullptr);
  EXPECT_EQ(web_contents_impl->GetColorProviderSourceForTesting(),
            default_source);
  web_contents()->GetColorProvider();
}

}  // namespace content
