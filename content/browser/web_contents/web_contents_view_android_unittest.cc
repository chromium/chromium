// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_view_android.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_refptr.h"
#include "base/no_destructor.h"
#include "base/run_loop.h"
#include "base/task/sequenced_task_runner.h"
#include "base/unguessable_token.h"
#include "content/browser/android/drop_data_android.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_drag_security_info.h"
#include "content/public/browser/clipboard_types.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_view_delegate.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fake_frame_widget.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "content/test/test_web_contents.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/drag_operation.h"
#include "third_party/blink/public/mojom/drag/drag.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/android/window_android.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"
#include "ui/color/color_provider.h"
#include "ui/events/android/drag_event_android.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/image/image_skia.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/jar_jni/DragEvent_jni.h"

namespace content {

namespace {

class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  explicit MockWebContentsDelegate(bool can_drag_enter)
      : can_drag_enter_(can_drag_enter) {}

  bool CanDragEnter(WebContents* source,
                    const DropData& data,
                    blink::DragOperationsMask operations_allowed) override {
    can_drag_enter_call_count_++;
    last_drop_data_ = data;
    return can_drag_enter_;
  }

  int can_drag_enter_call_count() const { return can_drag_enter_call_count_; }
  const DropData& last_drop_data() const { return last_drop_data_; }

 private:
  bool can_drag_enter_ = true;
  int can_drag_enter_call_count_ = 0;
  DropData last_drop_data_;
};

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

// A WebContentsViewDelegate that records OnPerformingDrop() calls and lets the
// test decide when and how each drop completes (including out-of-order across
// overlapping drops).
class FakeWebContentsViewDelegate : public WebContentsViewDelegate {
 public:
  void OnPerformingDrop(const DropData& drop_data,
                        DropCompletionCallback callback) override {
    on_performing_drop_call_count_++;
    last_drop_data_ = drop_data;
    pending_callbacks_.push_back(std::move(callback));
  }

  // Completes the oldest pending drop with `drop_data`, or aborts if nullopt.
  void CompletePendingDrop(std::optional<DropData> drop_data) {
    CompletePendingDropAt(0, std::move(drop_data));
  }

  // Completes the pending drop at `index` and removes it from the queue.
  void CompletePendingDropAt(size_t index, std::optional<DropData> drop_data) {
    ASSERT_LT(index, pending_callbacks_.size());
    DropCompletionCallback callback = std::move(pending_callbacks_[index]);
    pending_callbacks_.erase(pending_callbacks_.begin() + index);
    std::move(callback).Run(std::move(drop_data));
  }

  int on_performing_drop_call_count() const {
    return on_performing_drop_call_count_;
  }
  const DropData& last_drop_data() const { return last_drop_data_; }

 private:
  int on_performing_drop_call_count_ = 0;
  DropData last_drop_data_;
  std::deque<DropCompletionCallback> pending_callbacks_;
};

// A drag-and-drop call received by `DragTrackingFrameWidget`, in order.
struct DragCall {
  enum class Kind {
    kDragTargetDrop,
    // `DragTargetDrop` received after `DragSourceEndedAt` cancelled the
    // widget's drag session, which Blink ignores.
    kDragTargetDropIgnored,
    kDragTargetDragLeave,
    kDragSourceEndedAt,
  };

  Kind kind;
  // Set for `kDragSourceEndedAt`.
  ui::mojom::DragOperation operation = ui::mojom::DragOperation::kNone;

  bool operator==(const DragCall&) const = default;
};

void PrintTo(const DragCall& call, std::ostream* os) {
  switch (call.kind) {
    case DragCall::Kind::kDragTargetDrop:
      *os << "DragTargetDrop";
      return;
    case DragCall::Kind::kDragTargetDropIgnored:
      *os << "DragTargetDrop(ignored)";
      return;
    case DragCall::Kind::kDragTargetDragLeave:
      *os << "DragTargetDragLeave";
      return;
    case DragCall::Kind::kDragSourceEndedAt:
      *os << "DragSourceEndedAt(" << static_cast<int>(call.operation) << ")";
      return;
  }
}

// Intercepts FrameWidget drag-and-drop Mojo calls sent to Blink and runs
// response callbacks on `DragTargetDragEnter`, `DragTargetDragOver`, and
// `DragSourceEndedAt` (which `FakeFrameWidget`'s empty overrides otherwise drop
// while `receiver_` remains bound).
class DragTrackingFrameWidget : public FakeFrameWidget {
 public:
  explicit DragTrackingFrameWidget(
      mojo::PendingAssociatedReceiver<blink::mojom::FrameWidget> receiver)
      : FakeFrameWidget(mojo::NullAssociatedReceiver()),
        receiver_(this, std::move(receiver)) {}

  void FlushForTesting() { receiver_.FlushForTesting(); }

  void DragTargetDragEnter(blink::mojom::DragDataPtr drag_data,
                           const gfx::PointF& point_in_viewport,
                           const gfx::PointF& screen_point,
                           blink::DragOperationsMask operations_allowed,
                           uint32_t key_modifiers,
                           DragTargetDragEnterCallback callback) override {
    drag_enter_count_++;
    last_entered_item_count_ = drag_data ? drag_data->items.size() : 0u;
    std::move(callback).Run(ui::mojom::DragOperation::kCopy,
                            /*document_is_handling_drag=*/true);
  }

  void DragTargetDragOver(const gfx::PointF& point_in_viewport,
                          const gfx::PointF& screen_point,
                          blink::DragOperationsMask operations_allowed,
                          uint32_t modifiers,
                          DragTargetDragOverCallback callback) override {
    std::move(callback).Run(ui::mojom::DragOperation::kCopy,
                            /*document_is_handling_drag=*/true);
  }

  void DragTargetDragLeave(const gfx::PointF& point_in_viewport,
                           const gfx::PointF& screen_point) override {
    calls_.push_back({DragCall::Kind::kDragTargetDragLeave});
    drag_leave_count_++;
  }

  void DragTargetDrop(blink::mojom::DragDataPtr drag_data,
                      const gfx::PointF& point_in_viewport,
                      const gfx::PointF& screen_point,
                      uint32_t key_modifiers,
                      base::OnceClosure callback) override {
    // Mirror `WebFrameWidgetImpl::DragTargetDrop`, which ignores a drop once
    // `DragSourceEndedAt` has cancelled the widget's drag session.
    if (drag_session_cancelled_) {
      calls_.push_back({DragCall::Kind::kDragTargetDropIgnored});
      std::move(callback).Run();
      return;
    }
    calls_.push_back({DragCall::Kind::kDragTargetDrop});
    drop_count_++;
    last_dropped_text_.reset();
    if (drag_data) {
      for (const auto& item : drag_data->items) {
        if (item->is_string() &&
            item->get_string()->string_type == ui::kMimeTypePlainText) {
          last_dropped_text_ = item->get_string()->string_data;
        }
      }
    }
    std::move(callback).Run();
  }

  void DragSourceEndedAt(const gfx::PointF& client_point,
                         const gfx::PointF& screen_point,
                         ui::mojom::DragOperation operation,
                         base::OnceClosure callback) override {
    // Mirror `WebFrameWidgetImpl::DragSourceEndedAt`, which ends with
    // `DragSourceSystemDragEnded()` -> `CancelDrag()`.
    drag_session_cancelled_ = true;
    calls_.push_back({DragCall::Kind::kDragSourceEndedAt, operation});
    std::move(callback).Run();
  }

  int drag_enter_count() const { return drag_enter_count_; }
  size_t last_entered_item_count() const { return last_entered_item_count_; }
  int drag_leave_count() const { return drag_leave_count_; }
  int drop_count() const { return drop_count_; }
  const std::optional<std::u16string>& last_dropped_text() const {
    return last_dropped_text_;
  }
  // Drag-and-drop calls in the order Blink received them (drop, leave and
  // source-ended only).
  const std::vector<DragCall>& calls() const { return calls_; }

 private:
  mojo::AssociatedReceiver<blink::mojom::FrameWidget> receiver_;
  bool drag_session_cancelled_ = false;
  std::vector<DragCall> calls_;
  int drag_enter_count_ = 0;
  size_t last_entered_item_count_ = 0;
  int drag_leave_count_ = 0;
  int drop_count_ = 0;
  std::optional<std::u16string> last_dropped_text_;
};

// Named fields for MakeDragEvent(). Unset fields are null or empty, so tests
// only spell out what they care about.
struct DragEventParams {
  int action = 0;
  gfx::PointF location;
  gfx::PointF screen_location;
  // DragEventAndroid keeps a reference to the MIME types, so they must
  // outlive the event.
  raw_ptr<const std::vector<std::u16string>> mime_types = nullptr;
  bool has_files = false;
  base::android::ScopedJavaLocalRef<jobjectArray> filenames;
  base::android::ScopedJavaLocalRef<jstring> text;
  base::android::ScopedJavaLocalRef<jstring> html;
  base::android::ScopedJavaLocalRef<jstring> url;
  base::android::ScopedJavaLocalRef<jstring> custom_data;
  base::android::ScopedJavaLocalRef<jstring> effect_allowed;
  base::android::ScopedJavaLocalRef<jstring> drag_id;
};

ui::DragEventAndroid MakeDragEvent(const DragEventParams& params) {
  static const base::NoDestructor<std::vector<std::u16string>> kNoMimeTypes;
  return ui::DragEventAndroid(
      base::android::AttachCurrentThread(), params.action, params.location,
      params.screen_location,
      params.mime_types ? *params.mime_types : *kNoMimeTypes, params.has_files,
      params.filenames, params.text, params.html, params.url,
      params.custom_data, params.effect_allowed, params.drag_id);
}

}  // namespace

class WebContentsViewAndroidTest : public RenderViewHostTestHarness {
 public:
  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    const char kGoogleUrl[] = "https://google.com/";
    NavigateAndCommit(GURL(kGoogleUrl));

    view_ = std::make_unique<MockWebContentsViewAndroid>(test_web_contents(),
                                                         nullptr);
    test_web_contents()->set_delegate_view(view_.get());
  }

  void TearDown() override {
    test_web_contents()->SetDeferGetRenderWidgetHostAtPoint(false);
    test_web_contents()->set_delegate_view(nullptr);
    view_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  // Replaces the view under test with one owning `delegate`.
  void RecreateViewWithDelegate(
      std::unique_ptr<WebContentsViewDelegate> delegate) {
    test_web_contents()->set_delegate_view(nullptr);
    view_ = std::make_unique<MockWebContentsViewAndroid>(test_web_contents(),
                                                         std::move(delegate));
    test_web_contents()->set_delegate_view(view_.get());
  }

  // Simulates a drag entering the view and dropping `text` on it.
  void SimulateTextDrop(std::string_view text) {
    JNIEnv* env = base::android::AttachCurrentThread();
    std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};
    ui::DragEventAndroid enter_event =
        MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                       .mime_types = &mime_types});
    view()->OnDragEvent(enter_event);

    base::android::ScopedJavaLocalRef<jstring> j_text =
        base::android::ConvertUTF8ToJavaString(env, text);
    ui::DragEventAndroid drop_event =
        MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                       .location = gfx::PointF(10, 10),
                       .screen_location = gfx::PointF(10, 10),
                       .mime_types = &mime_types,
                       .text = j_text});
    view()->OnDragEvent(drop_event);
  }

  void SimulateDragEnded() {
    ui::DragEventAndroid end_event =
        MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENDED});
    view()->OnDragEvent(end_event);
  }

  void TriggerDeferredHitTest() {
    base::RunLoop run_loop;
    test_web_contents()
        ->TriggerGetRenderWidgetHostAtPointAsynchronouslyCallback();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
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

  TestWebContents* test_web_contents() {
    return static_cast<TestWebContents*>(web_contents());
  }

  MockWebContentsViewAndroid* view() { return view_.get(); }

 protected:
  using DragTargetSession = WebContentsViewAndroid::DragTargetSession;

  const DragTargetSession* active_drag() const {
    return view_->active_drag_.get();
  }

  const WebContentsViewDragSecurityInfo& drag_security_info() const {
    return view_->drag_security_info_;
  }

  void InitiateDragSecurityInfo(RenderWidgetHostImpl* source_rwh,
                                const DropData& drop_data) {
    view_->drag_security_info_.OnDragInitiated(source_rwh, drop_data);
  }

  // Puts the view in the state `StartDragging()` leaves it in once the system
  // drag has started from `source_rwh`. (The unit test harness has no Java
  // `ViewAndroidDelegate`, so `StartDragging()` itself cannot start one.)
  void SetDragSource(RenderWidgetHostImpl* source_rwh,
                     const DropData& drop_data) {
    view_->current_source_rwh_for_drag_ = source_rwh->GetWeakPtr();
    InitiateDragSecurityInfo(source_rwh, drop_data);
  }

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
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = 3,
                     .mime_types = &mime_types,
                     .custom_data = j_custom_data,
                     .effect_allowed = j_effect_allowed});

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
  InitiateDragSecurityInfo(GetRenderWidgetHost(), drop_data);

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with image MIME type.
  std::vector<std::u16string> enter_mime_types = {u"image/png"};
  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &enter_mime_types});
  view()->OnDragEvent(enter_event);

  // File metadata should not be advertised when image access is not allowed.
  ASSERT_TRUE(active_drag());
  EXPECT_TRUE(active_drag()->metadata.empty());

  // ACTION_DROP with filenames.
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://org.chromium.test/image.png", "image.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .mime_types = &enter_mime_types,
                     .filenames = j_filenames});
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
  InitiateDragSecurityInfo(GetRenderWidgetHost(), drop_data);

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with image MIME type.
  std::vector<std::u16string> enter_mime_types = {u"image/png"};
  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &enter_mime_types});
  view()->OnDragEvent(enter_event);

  // File metadata should be advertised when image access is allowed.
  ASSERT_TRUE(active_drag());
  ASSERT_EQ(active_drag()->metadata.size(), 1u);
  EXPECT_EQ(active_drag()->metadata[0].kind, DropData::Kind::FILENAME);
  EXPECT_EQ(active_drag()->metadata[0].filename, base::FilePath("file.png"));

  // ACTION_DROP with filenames.
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://org.chromium.test/image.png", "image.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .mime_types = &enter_mime_types,
                     .filenames = j_filenames});
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
  EXPECT_FALSE(drag_security_info().did_initiate());

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with image MIME type.
  std::vector<std::u16string> enter_mime_types = {u"image/png"};
  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &enter_mime_types});
  view()->OnDragEvent(enter_event);

  ASSERT_TRUE(active_drag());
  ASSERT_EQ(active_drag()->metadata.size(), 1u);
  EXPECT_EQ(active_drag()->metadata[0].kind, DropData::Kind::FILENAME);

  // ACTION_DROP with filenames.
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://external.app/photo.png", "photo.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .mime_types = &enter_mime_types,
                     .filenames = j_filenames});
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
  InitiateDragSecurityInfo(GetRenderWidgetHost(), drop_data);

  JNIEnv* env = base::android::AttachCurrentThread();

  // ACTION_DRAG_ENTERED with text and image MIME types.
  std::vector<std::u16string> enter_mime_types = {ui::kMimeTypePlainText16,
                                                  u"image/png"};
  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &enter_mime_types});
  view()->OnDragEvent(enter_event);

  // String MIME type is kept in metadata, while file metadata is omitted.
  ASSERT_TRUE(active_drag());
  ASSERT_EQ(active_drag()->metadata.size(), 1u);
  EXPECT_EQ(active_drag()->metadata[0].kind, DropData::Kind::STRING);
  EXPECT_EQ(active_drag()->metadata[0].mime_type, ui::kMimeTypePlainText16);

  // ACTION_DROP with text and filenames.
  base::android::ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF8ToJavaString(env, "sample text");
  std::vector<std::vector<std::string>> filenames_vec = {
      {"content://org.chromium.test/image.png", "image.png"}};
  base::android::ScopedJavaLocalRef<jobjectArray> j_filenames =
      base::android::ToJavaArrayOfStringArray(env, filenames_vec);
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .mime_types = &enter_mime_types,
                     .filenames = j_filenames,
                     .text = j_text});
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
  InitiateDragSecurityInfo(GetRenderWidgetHost(), drop_data);

  EXPECT_TRUE(drag_security_info().did_initiate());
  EXPECT_FALSE(drag_security_info().IsImageAccessibleFromFrame());

  ui::DragEventAndroid end_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENDED});

  view()->OnDragEvent(end_event);

  EXPECT_FALSE(drag_security_info().did_initiate());
  EXPECT_TRUE(drag_security_info().IsImageAccessibleFromFrame());
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

TEST_F(WebContentsViewAndroidTest,
       CanDragEnter_RejectedSuppressesDragEnterLocationAndDrop) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_view_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* view_delegate = owned_view_delegate.get();
  RecreateViewWithDelegate(std::move(owned_view_delegate));

  MockWebContentsDelegate delegate(/*can_drag_enter=*/false);
  web_contents()->SetDelegate(&delegate);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};

  ui::DragEventAndroid enter_event = MakeDragEvent(
      {.action = DragEventJni::ACTION_DRAG_ENTERED, .mime_types = &mime_types});
  view()->OnDragEvent(enter_event);

  EXPECT_EQ(delegate.can_drag_enter_call_count(), 1);
  EXPECT_TRUE(delegate.last_drop_data().text.has_value());
  ASSERT_TRUE(active_drag());
  EXPECT_TRUE(active_drag()->rejected_by_delegate);
  EXPECT_FALSE(active_drag()->target_rwh);

  ui::DragEventAndroid location_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_LOCATION,
                     .location = gfx::PointF(10, 10),
                     .screen_location = gfx::PointF(10, 10),
                     .mime_types = &mime_types});
  view()->OnDragEvent(location_event);
  EXPECT_EQ(delegate.can_drag_enter_call_count(), 1);
  ASSERT_TRUE(active_drag());
  EXPECT_FALSE(active_drag()->target_rwh);

  base::android::ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF8ToJavaString(env, "rejected text");
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .location = gfx::PointF(10, 10),
                     .screen_location = gfx::PointF(10, 10),
                     .mime_types = &mime_types,
                     .text = j_text});
  view()->OnDragEvent(drop_event);
  frame_widget.FlushForTesting();
  EXPECT_EQ(delegate.can_drag_enter_call_count(), 1);
  EXPECT_EQ(view_delegate->on_performing_drop_call_count(), 0);
  EXPECT_EQ(frame_widget.drag_enter_count(), 0);
  EXPECT_EQ(frame_widget.drop_count(), 0);
  ASSERT_TRUE(active_drag());
  EXPECT_FALSE(active_drag()->target_rwh);
  EXPECT_FALSE(view()->GetDropData());

  // Exiting destroys `active_drag_` so the next drag is evaluated afresh.
  ui::DragEventAndroid exit_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_EXITED});
  view()->OnDragEvent(exit_event);
  EXPECT_FALSE(active_drag());
  view()->OnDragEvent(enter_event);
  EXPECT_EQ(delegate.can_drag_enter_call_count(), 2);

  web_contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsViewAndroidTest, CanDragEnter_AcceptedSetsTargetRwh) {
  MockWebContentsDelegate delegate(/*can_drag_enter=*/true);
  web_contents()->SetDelegate(&delegate);

  std::vector<std::u16string> mime_types = {u"image/png"};

  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &mime_types,
                     .has_files = true});
  view()->OnDragEvent(enter_event);

  EXPECT_EQ(delegate.can_drag_enter_call_count(), 1);
  ASSERT_EQ(delegate.last_drop_data().filenames.size(), 1u);
  ASSERT_TRUE(active_drag());
  EXPECT_EQ(active_drag()->target_rwh.get(), GetRenderWidgetHost());

  web_contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsViewAndroidTest, DragIdRestoredFromJava) {
  MockWebContentsDelegate delegate(/*can_drag_enter=*/true);
  web_contents()->SetDelegate(&delegate);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {u"image/png"};
  const base::UnguessableToken drag_id = base::UnguessableToken::Create();
  base::android::ScopedJavaLocalRef<jstring> j_drag_id =
      base::android::ConvertUTF8ToJavaString(env, drag_id.ToString());

  // The drag ID is available at ACTION_DRAG_ENTERED, so the delegate sees it
  // in the provisional drop data.
  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &mime_types,
                     .has_files = true,
                     .drag_id = j_drag_id});
  view()->OnDragEvent(enter_event);
  EXPECT_EQ(delegate.can_drag_enter_call_count(), 1);
  EXPECT_EQ(delegate.last_drop_data().drag_id, drag_id);

  // It is also restored into the drop data on ACTION_DROP.
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .mime_types = &mime_types,
                     .has_files = true,
                     .drag_id = j_drag_id});
  view()->OnDragEvent(drop_event);
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->drag_id, drag_id);

  web_contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsViewAndroidTest, MalformedDragIdIgnored) {
  MockWebContentsDelegate delegate(/*can_drag_enter=*/true);
  web_contents()->SetDelegate(&delegate);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {u"image/png"};

  for (const char* malformed_id : {"not-a-token", ""}) {
    SCOPED_TRACE(malformed_id);
    base::android::ScopedJavaLocalRef<jstring> j_drag_id =
        base::android::ConvertUTF8ToJavaString(env, malformed_id);

    ui::DragEventAndroid enter_event =
        MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                       .mime_types = &mime_types,
                       .has_files = true,
                       .drag_id = j_drag_id});
    view()->OnDragEvent(enter_event);
    EXPECT_FALSE(delegate.last_drop_data().drag_id.has_value());

    ui::DragEventAndroid drop_event =
        MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                       .mime_types = &mime_types,
                       .has_files = true,
                       .drag_id = j_drag_id});
    view()->OnDragEvent(drop_event);
    ASSERT_TRUE(view()->GetDropData());
    EXPECT_FALSE(view()->GetDropData()->drag_id.has_value());

    ui::DragEventAndroid end_event =
        MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENDED});
    view()->OnDragEvent(end_event);
  }
  EXPECT_EQ(delegate.can_drag_enter_call_count(), 2);

  web_contents()->SetDelegate(nullptr);
}

TEST_F(WebContentsViewAndroidTest, OnDragEnded_ResetsDragId) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {u"image/png"};
  const base::UnguessableToken drag_id = base::UnguessableToken::Create();
  base::android::ScopedJavaLocalRef<jstring> j_drag_id =
      base::android::ConvertUTF8ToJavaString(env, drag_id.ToString());

  ui::DragEventAndroid enter_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENTERED,
                     .mime_types = &mime_types,
                     .has_files = true,
                     .drag_id = j_drag_id});
  view()->OnDragEvent(enter_event);
  ASSERT_TRUE(active_drag());
  EXPECT_EQ(active_drag()->drag_id, drag_id);

  // Android provides no ClipDescription, so no drag ID, on ACTION_DRAG_ENDED.
  ui::DragEventAndroid end_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENDED});
  view()->OnDragEvent(end_event);
  EXPECT_FALSE(active_drag());
}

// Checks that a drag ID survives the trip out to Java (as
// `DropDataAndroid.dragId`, which DragAndDropDelegateImpl copies into the
// ClipDescription extras) and back in through a drag event.
TEST(DropDataAndroidTest, DragIdRoundTripsThroughJava) {
  JNIEnv* env = base::android::AttachCurrentThread();
  DropData drop_data;
  drop_data.drag_id = base::UnguessableToken::Create();

  base::android::ScopedJavaLocalRef<jobject> j_drop_data =
      ToJavaDropData(drop_data);
  ASSERT_FALSE(j_drop_data.is_null());
  base::android::ScopedJavaLocalRef<jclass> j_class =
      base::android::ScopedJavaLocalRef<jclass>::Adopt(
          env, env->GetObjectClass(j_drop_data.obj()));
  jfieldID drag_id_field =
      env->GetFieldID(j_class.obj(), "dragId", "Ljava/lang/String;");
  ASSERT_TRUE(drag_id_field);
  base::android::ScopedJavaLocalRef<jstring> j_drag_id =
      base::android::ScopedJavaLocalRef<jstring>::Adopt(
          env, static_cast<jstring>(
                   env->GetObjectField(j_drop_data.obj(), drag_id_field)));
  ASSERT_FALSE(j_drag_id.is_null());

  ui::DragEventAndroid drop_event = MakeDragEvent(
      {.action = DragEventJni::ACTION_DROP, .drag_id = j_drag_id});
  EXPECT_EQ(ParseDragIdFromEvent(drop_event), drop_data.drag_id);
}

TEST(DropDataAndroidTest, NoDragIdInJavaWithoutOne) {
  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_drop_data =
      ToJavaDropData(DropData());
  ASSERT_FALSE(j_drop_data.is_null());
  base::android::ScopedJavaLocalRef<jclass> j_class =
      base::android::ScopedJavaLocalRef<jclass>::Adopt(
          env, env->GetObjectClass(j_drop_data.obj()));
  jfieldID drag_id_field =
      env->GetFieldID(j_class.obj(), "dragId", "Ljava/lang/String;");
  ASSERT_TRUE(drag_id_field);
  base::android::ScopedJavaLocalRef<jobject> j_drag_id =
      base::android::ScopedJavaLocalRef<jobject>::Adopt(
          env, env->GetObjectField(j_drop_data.obj(), drag_id_field));
  EXPECT_TRUE(j_drag_id.is_null());
}

TEST_F(WebContentsViewAndroidTest, UpdateDragOperation_IgnoresStaleTargetRwh) {
  std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};

  ui::DragEventAndroid enter_event = MakeDragEvent(
      {.action = DragEventJni::ACTION_DRAG_ENTERED, .mime_types = &mime_types});
  view()->OnDragEvent(enter_event);
  ASSERT_TRUE(active_drag());
  ASSERT_EQ(active_drag()->target_rwh.get(), GetRenderWidgetHost());

  // Update from the current target RWH succeeds.
  view()->UpdateDragOperation(ui::mojom::DragOperation::kCopy,
                              /*document_is_handling_drag=*/true,
                              GetRenderWidgetHost());
  EXPECT_EQ(active_drag()->operation, ui::mojom::DragOperation::kCopy);
  EXPECT_TRUE(active_drag()->document_is_handling_drag);

  // UpdateDragOperation from a non-current or null RWH must not overwrite the
  // current target RWH's state.
  std::unique_ptr<WebContents> other_web_contents = CreateTestWebContents();
  RenderWidgetHostImpl* other_rwh = static_cast<RenderWidgetHostImpl*>(
      other_web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
  view()->UpdateDragOperation(ui::mojom::DragOperation::kNone,
                              /*document_is_handling_drag=*/false, other_rwh);
  view()->UpdateDragOperation(ui::mojom::DragOperation::kNone,
                              /*document_is_handling_drag=*/false, nullptr);
  EXPECT_EQ(active_drag()->operation, ui::mojom::DragOperation::kCopy);
  EXPECT_TRUE(active_drag()->document_is_handling_drag);

  // Exiting the drag destroys `active_drag_` and ignores late replies arriving
  // after exit.
  ui::DragEventAndroid exit_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_EXITED});
  view()->OnDragEvent(exit_event);
  EXPECT_FALSE(active_drag());

  view()->UpdateDragOperation(ui::mojom::DragOperation::kCopy,
                              /*document_is_handling_drag=*/true,
                              GetRenderWidgetHost());
  EXPECT_FALSE(active_drag());

  // Re-entering and then ending the drag also destroys `active_drag_` and
  // ignores late replies arriving after end.
  view()->OnDragEvent(enter_event);
  view()->UpdateDragOperation(ui::mojom::DragOperation::kCopy,
                              /*document_is_handling_drag=*/true,
                              GetRenderWidgetHost());
  ASSERT_TRUE(active_drag());
  EXPECT_EQ(active_drag()->operation, ui::mojom::DragOperation::kCopy);
  EXPECT_TRUE(active_drag()->document_is_handling_drag);

  ui::DragEventAndroid end_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DRAG_ENDED});
  view()->OnDragEvent(end_event);
  EXPECT_FALSE(active_drag());

  view()->UpdateDragOperation(ui::mojom::DragOperation::kCopy,
                              /*document_is_handling_drag=*/true,
                              GetRenderWidgetHost());
  EXPECT_FALSE(active_drag());
}

// Without a WebContentsViewDelegate the drop is dispatched to the renderer
// directly.
TEST_F(WebContentsViewAndroidTest, OnPerformingDrop_NoDelegateDispatchesDrop) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));

  SimulateTextDrop("dropped text");
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drop_count(), 1);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"dropped text");
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->view_id,
            GetRenderWidgetHost()->GetRoutingID());
}

// The default WebContentsViewDelegate::OnPerformingDrop() implementation
// synchronously passes the drop through unchanged.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_DefaultDelegatePassesDropThrough) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  RecreateViewWithDelegate(std::make_unique<WebContentsViewDelegate>());

  SimulateTextDrop("default delegate text");
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drop_count(), 1);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"default delegate text");
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->text, u"default delegate text");
  EXPECT_EQ(view()->GetDropData()->view_id,
            GetRenderWidgetHost()->GetRoutingID());
}

// `FilterDropData` runs on the target RenderWidgetHost before
// `delegate_->OnPerformingDrop()`, matching Aura and Mac so delegates see
// renderer-filtered URLs and `view_id`.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_FiltersDropDataBeforeDelegate) {
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {ui::kMimeTypeMozillaUrl16};
  ui::DragEventAndroid enter_event = MakeDragEvent(
      {.action = DragEventJni::ACTION_DRAG_ENTERED, .mime_types = &mime_types});
  view()->OnDragEvent(enter_event);

  base::android::ScopedJavaLocalRef<jstring> j_url =
      base::android::ConvertUTF8ToJavaString(env, "javascript:alert(1)");
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .location = gfx::PointF(10, 10),
                     .screen_location = gfx::PointF(10, 10),
                     .mime_types = &mime_types,
                     .url = j_url});
  view()->OnDragEvent(drop_event);

  ASSERT_EQ(delegate->on_performing_drop_call_count(), 1);
  EXPECT_EQ(delegate->last_drop_data().view_id,
            GetRenderWidgetHost()->GetRoutingID());
  ASSERT_EQ(delegate->last_drop_data().url_infos.size(), 1u);
  EXPECT_EQ(delegate->last_drop_data().url_infos[0].url, GURL(kBlockedURL));
}

TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_DispatchesDelegateModifiedDropData) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  SimulateTextDrop("original text");
  frame_widget.FlushForTesting();

  // The delegate sees the drop data and the drop waits for its completion
  // before reaching Blink or populating `GetDropData()`.
  EXPECT_EQ(delegate->on_performing_drop_call_count(), 1);
  EXPECT_EQ(delegate->last_drop_data().text, u"original text");
  EXPECT_EQ(frame_widget.drop_count(), 0);
  EXPECT_FALSE(view()->GetDropData());

  DropData modified_drop_data = delegate->last_drop_data();
  modified_drop_data.text = u"modified text";
  delegate->CompletePendingDrop(std::move(modified_drop_data));
  frame_widget.FlushForTesting();

  // The delegate-modified data is what gets dispatched to the renderer.
  EXPECT_EQ(frame_widget.drop_count(), 1);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"modified text");
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->text, u"modified text");
  EXPECT_EQ(view()->GetDropData()->view_id,
            GetRenderWidgetHost()->GetRoutingID());
}

TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_AbortLeavesDragWithoutDrop) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  SimulateTextDrop("aborted text");
  EXPECT_EQ(delegate->on_performing_drop_call_count(), 1);
  ASSERT_TRUE(active_drag());
  EXPECT_EQ(active_drag()->target_rwh.get(), GetRenderWidgetHost());

  delegate->CompletePendingDrop(std::nullopt);
  frame_widget.FlushForTesting();

  // The drop is never dispatched and the renderer's drag session is left, as
  // if the drag had exited the view.
  EXPECT_EQ(frame_widget.drag_leave_count(), 1);
  EXPECT_EQ(frame_widget.drop_count(), 0);
  EXPECT_FALSE(view()->GetDropData());
  EXPECT_FALSE(active_drag());

  SimulateDragEnded();
  EXPECT_FALSE(active_drag());
}

// An asynchronous `OnPerformingDrop()` completion that arrives after
// `ACTION_DRAG_ENDED` still dispatches the drop it was given.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_CompletedAfterDragEndedStillDispatchesDrop) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  SimulateTextDrop("late text");
  EXPECT_EQ(delegate->on_performing_drop_call_count(), 1);

  SimulateDragEnded();
  EXPECT_FALSE(view()->GetDropData());

  delegate->CompletePendingDrop(delegate->last_drop_data());
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drop_count(), 1);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"late text");
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->text, u"late text");
  EXPECT_EQ(view()->GetDropData()->view_id,
            GetRenderWidgetHost()->GetRoutingID());
}

// On Android, `OnDragEnded()` destroys `active_drag_` without sending
// `DragTargetDragLeave`. If an asynchronous `OnPerformingDrop()` callback
// aborts after `ACTION_DRAG_ENDED`, `DragTargetDragLeave` must still reach the
// target RenderWidgetHost so Blink dismisses its drag-over state.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_AsyncAbortAfterDragEndedSendsDragLeave) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  SimulateTextDrop("late aborted text");
  EXPECT_EQ(delegate->on_performing_drop_call_count(), 1);

  SimulateDragEnded();
  frame_widget.FlushForTesting();
  EXPECT_EQ(frame_widget.drag_leave_count(), 0);

  delegate->CompletePendingDrop(std::nullopt);
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drag_leave_count(), 1);
  EXPECT_EQ(frame_widget.drop_count(), 0);
  EXPECT_FALSE(view()->GetDropData());
}

// If a second drag enters the view before the first drop's asynchronous
// `OnPerformingDrop()` callback aborts, aborting the first drop must not exit
// or send `DragTargetDragLeave` to the active second drag session.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_AsyncAbortDoesNotExitNewerDragSession) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  SimulateTextDrop("first drag");
  EXPECT_EQ(delegate->on_performing_drop_call_count(), 1);
  SimulateDragEnded();

  // Start a second drag over the same RenderWidgetHost.
  std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};
  ui::DragEventAndroid enter_event = MakeDragEvent(
      {.action = DragEventJni::ACTION_DRAG_ENTERED, .mime_types = &mime_types});
  view()->OnDragEvent(enter_event);
  view()->UpdateDragOperation(ui::mojom::DragOperation::kCopy,
                              /*document_is_handling_drag=*/true,
                              GetRenderWidgetHost());
  ASSERT_TRUE(active_drag());
  ASSERT_EQ(active_drag()->target_rwh.get(), GetRenderWidgetHost());

  // Abort the first drop asynchronously while the second drag is active.
  delegate->CompletePendingDrop(std::nullopt);
  frame_widget.FlushForTesting();

  ASSERT_TRUE(active_drag());
  EXPECT_EQ(active_drag()->target_rwh.get(), GetRenderWidgetHost());
  EXPECT_EQ(active_drag()->operation, ui::mojom::DragOperation::kCopy);
  EXPECT_TRUE(active_drag()->document_is_handling_drag);
  EXPECT_EQ(frame_widget.drag_leave_count(), 0);
  EXPECT_EQ(frame_widget.drop_count(), 0);
}

// Overlapping asynchronous drops each carry their own `OnPerformingDropContext`
// and isolated `DropData` payload, so out-of-order completion dispatches each
// drop's payload to Blink and updates `GetDropData()` to the drop just
// dispatched.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_OverlappingDropsIsolateDropData) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  SimulateTextDrop("first drop");
  ASSERT_EQ(delegate->on_performing_drop_call_count(), 1);
  DropData first_drop_data = delegate->last_drop_data();
  SimulateDragEnded();
  EXPECT_FALSE(view()->GetDropData());

  // Perform and complete a second drop while the first callback is still
  // pending.
  SimulateTextDrop("second drop");
  ASSERT_EQ(delegate->on_performing_drop_call_count(), 2);
  delegate->CompletePendingDropAt(1, delegate->last_drop_data());
  SimulateDragEnded();
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drop_count(), 1);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"second drop");
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->text, u"second drop");

  // Complete the first drop late with modified text; its own drop context was
  // not overwritten by the second drop.
  first_drop_data.text = u"first drop modified";
  delegate->CompletePendingDropAt(0, std::move(first_drop_data));
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drop_count(), 2);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"first drop modified");
  ASSERT_TRUE(view()->GetDropData());
  EXPECT_EQ(view()->GetDropData()->text, u"first drop modified");
}

// `OnDragEnded()` resets `view()->drag_security_info_` at `ACTION_DRAG_ENDED`,
// so `GotModifiedDropDataFromDelegate()` must re-validate against the
// `WebContentsViewDragSecurityInfo` snapshotted at `ACTION_DROP`.
TEST_F(WebContentsViewAndroidTest,
       OnPerformingDrop_AsyncCompletionRevalidatesSnapshottedDragSecurityInfo) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  // Initiate a drag from a cross-site page hosted in another WebContents
  // (mirroring an embedded guest page). While `other_web_contents` is alive,
  // `IsValidDragTarget()` allows the cross-page drag into `view()`.
  constexpr char kOtherOriginUrl[] = "https://other-origin.com/";
  std::unique_ptr<WebContents> other_web_contents =
      WebContentsTester::CreateTestWebContents(
          browser_context(),
          SiteInstance::CreateForURL(browser_context(), GURL(kOtherOriginUrl)));
  WebContentsTester::For(other_web_contents.get())
      ->NavigateAndCommit(GURL(kOtherOriginUrl));
  RenderWidgetHostImpl* other_rwh = static_cast<RenderWidgetHostImpl*>(
      other_web_contents->GetPrimaryMainFrame()->GetRenderWidgetHost());
  ASSERT_NE(other_rwh->GetSiteInstanceGroup()->GetId(),
            GetRenderWidgetHost()->GetSiteInstanceGroup()->GetId());

  InitiateDragSecurityInfo(other_rwh, DropData());
  ASSERT_TRUE(drag_security_info().IsValidDragTarget(GetRenderWidgetHost()));

  SimulateTextDrop("cross-page drop");
  ASSERT_EQ(delegate->on_performing_drop_call_count(), 1);

  // `ACTION_DRAG_ENDED` resets `view()->drag_security_info_`.
  SimulateDragEnded();
  EXPECT_FALSE(drag_security_info().did_initiate());

  // Destroying the source WebContents invalidates `source_rwh_` in the
  // snapshotted `WebContentsViewDragSecurityInfo`, so `IsValidDragTarget()` no
  // longer treats the cross-SiteInstanceGroup target as a valid cross-page
  // drop target.
  other_web_contents.reset();

  delegate->CompletePendingDrop(delegate->last_drop_data());
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.drop_count(), 0);
  EXPECT_FALSE(view()->GetDropData());
}

// If `ACTION_DRAG_ENDED` clears `active_drag_` while `OnPerformDrop()`'s async
// hit-test is still in flight, `PerformDropCallback()` must still run
// `CanDragEnter` against the `DragTargetSession` retained by
// `OnPerformingDropContext` (including metadata parsed lazily on `ACTION_DROP`)
// and suppress rejected drops.
TEST_F(WebContentsViewAndroidTest,
       PerformDropCallback_AfterDragEndedEnforcesCanDragEnter) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_view_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* view_delegate = owned_view_delegate.get();
  RecreateViewWithDelegate(std::move(owned_view_delegate));

  MockWebContentsDelegate web_contents_delegate(/*can_drag_enter=*/false);
  web_contents()->SetDelegate(&web_contents_delegate);
  test_web_contents()->SetDeferGetRenderWidgetHostAtPoint(true);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};
  base::android::ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF8ToJavaString(env, "blocked lazy drop");
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .location = gfx::PointF(10, 10),
                     .screen_location = gfx::PointF(10, 10),
                     .mime_types = &mime_types,
                     .text = j_text});
  view()->OnDragEvent(drop_event);

  // `ACTION_DRAG_ENDED` arrives before the async hit-test callback runs.
  SimulateDragEnded();
  EXPECT_FALSE(active_drag());
  EXPECT_EQ(web_contents_delegate.can_drag_enter_call_count(), 0);

  TriggerDeferredHitTest();
  frame_widget.FlushForTesting();

  EXPECT_EQ(web_contents_delegate.can_drag_enter_call_count(), 1);
  EXPECT_TRUE(web_contents_delegate.last_drop_data().text.has_value());
  EXPECT_EQ(view_delegate->on_performing_drop_call_count(), 0);
  EXPECT_EQ(frame_widget.drag_enter_count(), 0);
  EXPECT_EQ(frame_widget.drop_count(), 0);
  EXPECT_FALSE(view()->GetDropData());

  web_contents()->SetDelegate(nullptr);
}

// If `ACTION_DRAG_ENDED` clears `active_drag_` before `PerformDropCallback()`
// enters a target RenderWidgetHost that had not yet received
// `DragTargetDragEnter`, `PerformDropCallback()` still enters that target with
// parsed metadata before dispatching `DragTargetDrop`.
TEST_F(WebContentsViewAndroidTest,
       PerformDropCallback_AfterDragEndedEntersTargetWithMetadataBeforeDrop) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  RecreateViewWithDelegate(std::make_unique<WebContentsViewDelegate>());

  MockWebContentsDelegate web_contents_delegate(/*can_drag_enter=*/true);
  web_contents()->SetDelegate(&web_contents_delegate);
  test_web_contents()->SetDeferGetRenderWidgetHostAtPoint(true);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};
  base::android::ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF8ToJavaString(env, "lazy async drop");
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .location = gfx::PointF(10, 10),
                     .screen_location = gfx::PointF(10, 10),
                     .mime_types = &mime_types,
                     .text = j_text});
  view()->OnDragEvent(drop_event);

  SimulateDragEnded();
  EXPECT_FALSE(active_drag());

  TriggerDeferredHitTest();
  frame_widget.FlushForTesting();

  EXPECT_EQ(web_contents_delegate.can_drag_enter_call_count(), 1);
  EXPECT_TRUE(web_contents_delegate.last_drop_data().text.has_value());
  EXPECT_EQ(frame_widget.drag_enter_count(), 1);
  EXPECT_EQ(frame_widget.last_entered_item_count(), 1u);
  EXPECT_EQ(frame_widget.drop_count(), 1);
  EXPECT_EQ(frame_widget.last_dropped_text(), u"lazy async drop");

  web_contents()->SetDelegate(nullptr);
}

// A failed outgoing `StartDragging()` attempt must not wipe an active incoming
// `DragTargetSession` (or its latched `rejected_by_delegate` decision).
TEST_F(WebContentsViewAndroidTest,
       StartDragging_FailurePreservesActiveIncomingDragSession) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_view_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* view_delegate = owned_view_delegate.get();
  RecreateViewWithDelegate(std::move(owned_view_delegate));

  MockWebContentsDelegate web_contents_delegate(/*can_drag_enter=*/false);
  web_contents()->SetDelegate(&web_contents_delegate);

  JNIEnv* env = base::android::AttachCurrentThread();
  std::vector<std::u16string> mime_types = {ui::kMimeTypePlainText16};
  ui::DragEventAndroid enter_event = MakeDragEvent(
      {.action = DragEventJni::ACTION_DRAG_ENTERED, .mime_types = &mime_types});
  view()->OnDragEvent(enter_event);
  ASSERT_TRUE(active_drag());
  EXPECT_TRUE(active_drag()->rejected_by_delegate);
  EXPECT_EQ(web_contents_delegate.can_drag_enter_call_count(), 1);

  // Trigger an outgoing drag start failure while the incoming drag is active.
  view()->set_allowed(false);
  DropData outgoing_drop_data;
  outgoing_drop_data.text = u"Blocked outgoing drag";
  view()->StartDragging(*web_contents()->GetPrimaryMainFrame(),
                        outgoing_drop_data, blink::kDragOperationCopy,
                        CreateValidDragImage(), gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo());
  EXPECT_TRUE(view()->system_drag_ended_called());

  ASSERT_TRUE(active_drag());
  EXPECT_TRUE(active_drag()->rejected_by_delegate);

  base::android::ScopedJavaLocalRef<jstring> j_text =
      base::android::ConvertUTF8ToJavaString(env, "still rejected");
  ui::DragEventAndroid drop_event =
      MakeDragEvent({.action = DragEventJni::ACTION_DROP,
                     .location = gfx::PointF(10, 10),
                     .screen_location = gfx::PointF(10, 10),
                     .mime_types = &mime_types,
                     .text = j_text});
  view()->OnDragEvent(drop_event);
  frame_widget.FlushForTesting();

  EXPECT_EQ(web_contents_delegate.can_drag_enter_call_count(), 1);
  EXPECT_EQ(view_delegate->on_performing_drop_call_count(), 0);
  EXPECT_EQ(frame_widget.drop_count(), 0);

  web_contents()->SetDelegate(nullptr);
}

// When the drag source and drop target are the same widget and the drop
// completes asynchronously after `ACTION_DRAG_ENDED`, the renderer must still
// receive `DragTargetDrop` before `DragSourceEndedAt`: Blink's
// `DragSourceEndedAt` cancels the widget's drag session, after which it ignores
// `DragTargetDrop`.
TEST_F(WebContentsViewAndroidTest,
       OnDragEnded_DefersDragSourceEndedAtUntilAsyncDropCompletes) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  DropData source_drop_data;
  source_drop_data.text = u"same-page text";
  SetDragSource(GetRenderWidgetHost(), source_drop_data);

  SimulateTextDrop("same-page text");
  // Deliver the renderer's `DragTargetDragEnter` reply (operation = kCopy)
  // while the drag is still active, as happens long before a real drop.
  frame_widget.FlushForTesting();
  EXPECT_EQ(delegate->on_performing_drop_call_count(), 1);
  EXPECT_TRUE(frame_widget.calls().empty());

  SimulateDragEnded();
  frame_widget.FlushForTesting();
  EXPECT_TRUE(frame_widget.calls().empty());
  EXPECT_FALSE(view()->system_drag_ended_called());

  // The source renderer's drag session is still open for the pending drop, so
  // a new drag from this view must be refused until the deferred source
  // notification has been sent. It is refused before any drag-start work, so
  // neither the policy check nor `ClearDragStateOnStartFailure()` runs.
  view()->set_allowed(true);
  DropData new_drag_data;
  new_drag_data.text = u"reentrant drag";
  view()->StartDragging(*web_contents()->GetPrimaryMainFrame(), new_drag_data,
                        blink::kDragOperationCopy, CreateValidDragImage(),
                        gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo());
  EXPECT_FALSE(view()->was_called());
  EXPECT_FALSE(view()->system_drag_ended_called());
  frame_widget.FlushForTesting();
  EXPECT_TRUE(frame_widget.calls().empty());

  delegate->CompletePendingDrop(delegate->last_drop_data());
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.calls(),
            (std::vector<DragCall>{{DragCall::Kind::kDragTargetDrop},
                                   {DragCall::Kind::kDragSourceEndedAt,
                                    ui::mojom::DragOperation::kCopy}}));
  EXPECT_EQ(frame_widget.last_dropped_text(), u"same-page text");
  EXPECT_TRUE(view()->system_drag_ended_called());

  // Once the deferred notification has been sent, new drags start again. (In
  // this harness the system drag cannot be started, so `StartDragging()` gets
  // as far as the policy check and then clears its state.)
  view()->StartDragging(*web_contents()->GetPrimaryMainFrame(), new_drag_data,
                        blink::kDragOperationCopy, CreateValidDragImage(),
                        gfx::Vector2d(), gfx::Rect(),
                        blink::mojom::DragEventSourceInfo());
  EXPECT_TRUE(view()->was_called());
}

// If the asynchronous drop is aborted after `ACTION_DRAG_ENDED`, the deferred
// source notification reports `kNone` and follows `DragTargetDragLeave`.
TEST_F(WebContentsViewAndroidTest,
       OnDragEnded_DeferredDragSourceEndedAtReportsNoneOnAbort) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  auto owned_delegate = std::make_unique<FakeWebContentsViewDelegate>();
  FakeWebContentsViewDelegate* delegate = owned_delegate.get();
  RecreateViewWithDelegate(std::move(owned_delegate));

  DropData source_drop_data;
  source_drop_data.text = u"aborted same-page text";
  SetDragSource(GetRenderWidgetHost(), source_drop_data);

  SimulateTextDrop("aborted same-page text");
  frame_widget.FlushForTesting();
  SimulateDragEnded();
  frame_widget.FlushForTesting();
  EXPECT_TRUE(frame_widget.calls().empty());

  delegate->CompletePendingDrop(std::nullopt);
  frame_widget.FlushForTesting();

  EXPECT_EQ(frame_widget.calls(),
            (std::vector<DragCall>{{DragCall::Kind::kDragTargetDragLeave},
                                   {DragCall::Kind::kDragSourceEndedAt,
                                    ui::mojom::DragOperation::kNone}}));
  EXPECT_EQ(frame_widget.drop_count(), 0);
  EXPECT_TRUE(view()->system_drag_ended_called());
}

// A drop that completes synchronously (the default delegate) is not affected:
// the source is notified from `OnDragEnded()` as before, after the drop.
TEST_F(WebContentsViewAndroidTest,
       OnDragEnded_SynchronousDropNotifiesSourceImmediately) {
  DragTrackingFrameWidget frame_widget(
      BindFakeFrameWidgetInterfaces(main_rfh()));
  RecreateViewWithDelegate(std::make_unique<WebContentsViewDelegate>());

  DropData source_drop_data;
  source_drop_data.text = u"sync same-page text";
  SetDragSource(GetRenderWidgetHost(), source_drop_data);

  SimulateTextDrop("sync same-page text");
  frame_widget.FlushForTesting();
  EXPECT_EQ(frame_widget.calls(),
            (std::vector<DragCall>{{DragCall::Kind::kDragTargetDrop}}));

  SimulateDragEnded();
  frame_widget.FlushForTesting();
  EXPECT_EQ(frame_widget.calls(),
            (std::vector<DragCall>{{DragCall::Kind::kDragTargetDrop},
                                   {DragCall::Kind::kDragSourceEndedAt,
                                    ui::mojom::DragOperation::kCopy}}));
  EXPECT_TRUE(view()->system_drag_ended_called());
}

}  // namespace content
