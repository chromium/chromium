// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include <stdint.h>

#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/threading/scoped_blocking_call.h"
#include "cc/input/android/offset_tag_android.h"
#include "cc/input/browser_controls_offset_tags_info.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/renderer_host/view_transition_opt_in_state.h"
#include "content/browser/web_contents/view_structure_builder_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame.mojom.h"
#include "content/public/browser/back_forward_transition_animation_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/isolated_world_ids.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-blink.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/ax_tree_update.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/snapshot/snapshot.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/WebContentsImpl_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::RunRunnableAndroid;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaArrayOfStringArray;
using base::android::ToJavaArrayOfStrings;
using base::android::ToJavaIntArray;

namespace content {

namespace {

// Track all WebContentsAndroid objects here so that we don't deserialize a
// destroyed WebContents object.
base::LazyInstance<std::unordered_set<WebContentsAndroid*>>::Leaky
    g_allocated_web_contents_androids = LAZY_INSTANCE_INITIALIZER;

void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              base::Value result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_WebContentsImpl_onEvaluateJavaScriptResult(env, j_json, callback);
}

void SmartClipCallback(const ScopedJavaGlobalRef<jobject>& callback,
                       const std::u16string& text,
                       const std::u16string& html,
                       const gfx::Rect& clip_rect) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_text = ConvertUTF16ToJavaString(env, text);
  ScopedJavaLocalRef<jstring> j_html = ConvertUTF16ToJavaString(env, html);
  Java_WebContentsImpl_onSmartClipDataExtracted(
      env, j_text, j_html, clip_rect.x(), clip_rect.y(), clip_rect.right(),
      clip_rect.bottom(), callback);
}

void CreateJavaAXSnapshot(JNIEnv* env,
                          const ui::AssistantTree* tree,
                          const ui::AssistantNode* node,
                          const JavaRef<jobject>& j_view_structure_node,
                          const JavaRef<jobject>& j_view_structure_builder,
                          bool is_root) {
  ScopedJavaLocalRef<jstring> j_text =
      ConvertUTF16ToJavaString(env, node->text);

  // The (fake) Android java class name.
  ScopedJavaLocalRef<jstring> j_class =
      ConvertUTF8ToJavaString(env, node->class_name);

  bool has_selection = node->selection.has_value();
  int sel_start = has_selection ? node->selection->start() : 0;
  int sel_end = has_selection ? node->selection->end() : 0;
  int child_count = static_cast<int>(node->children_indices.size());

  ViewStructureBuilder_populateViewStructureNode(
      env, j_view_structure_builder, j_view_structure_node, j_text,
      has_selection, sel_start, sel_end, node->color, node->bgcolor,
      node->text_size, node->bold, node->italic, node->underline,
      node->line_through, j_class, child_count);

  // Bounding box.
  ViewStructureBuilder_setViewStructureNodeBounds(
      env, j_view_structure_builder, j_view_structure_node, is_root,
      node->rect.x(), node->rect.y(), node->rect.width(), node->rect.height(),
      node->unclipped_rect.x(), node->unclipped_rect.y(),
      node->unclipped_rect.width(), node->unclipped_rect.height(),
      node->page_absolute_rect.x(), node->page_absolute_rect.y(),
      node->page_absolute_rect.width(), node->page_absolute_rect.height());

  // HTML/CSS attributes.
  ScopedJavaLocalRef<jstring> j_html_tag =
      ConvertUTF8ToJavaString(env, node->html_tag);
  ScopedJavaLocalRef<jstring> j_css_display =
      ConvertUTF8ToJavaString(env, node->css_display);
  std::vector<std::vector<std::u16string>> html_attrs;
  for (const auto& attr : node->html_attributes) {
    html_attrs.push_back(
        {base::UTF8ToUTF16(attr.first), base::UTF8ToUTF16(attr.second)});
  }
  ScopedJavaLocalRef<jobjectArray> j_attrs =
      ToJavaArrayOfStringArray(env, html_attrs);

  ViewStructureBuilder_setViewStructureNodeHtmlInfo(
      env, j_view_structure_builder, j_view_structure_node, j_html_tag,
      j_css_display, j_attrs);

  for (int child_index = 0; child_index < child_count; child_index++) {
    int child_id = node->children_indices[child_index];
    ScopedJavaLocalRef<jobject> j_child =
        ViewStructureBuilder_addViewStructureNodeChild(
            env, j_view_structure_builder, j_view_structure_node, child_index);
    CreateJavaAXSnapshot(env, tree, tree->nodes[child_id].get(), j_child,
                         j_view_structure_builder, false);
  }

  if (!is_root) {
    ViewStructureBuilder_commitViewStructureNode(env, j_view_structure_builder,
                                                 j_view_structure_node);
  }
}

void AddTreeLevelDataToViewStructure(
    JNIEnv* env,
    const JavaRef<jobject>& view_structure_root,
    const JavaRef<jobject>& view_structure_builder,
    const ui::AXTreeUpdate& ax_tree_update) {
  const auto& metadata_strings = ax_tree_update.tree_data.metadata;
  if (metadata_strings.empty())
    return;

  ScopedJavaLocalRef<jobjectArray> j_metadata_strings =
      ToJavaArrayOfStrings(env, metadata_strings);
  ViewStructureBuilder_setViewStructureNodeHtmlMetadata(
      env, view_structure_builder, view_structure_root, j_metadata_strings);
}

}  // namespace

// static
WebContents* WebContents::FromJavaWebContents(
    const JavaRef<jobject>& jweb_contents_android) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (jweb_contents_android.is_null())
    return NULL;

  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(
          Java_WebContentsImpl_getNativePointer(AttachCurrentThread(),
                                                jweb_contents_android));
  if (!web_contents_android)
    return NULL;
  return web_contents_android->web_contents();
}

// static
static void JNI_WebContentsImpl_DestroyWebContents(
    JNIEnv* env,
    jlong jweb_contents_android_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(jweb_contents_android_ptr);
  if (!web_contents_android)
    return;

  WebContents* web_contents = web_contents_android->web_contents();
  if (!web_contents)
    return;

  delete web_contents;
}

// static
ScopedJavaLocalRef<jobject> JNI_WebContentsImpl_FromNativePtr(
    JNIEnv* env,
    jlong web_contents_ptr) {
  WebContentsAndroid* web_contents_android =
      reinterpret_cast<WebContentsAndroid*>(web_contents_ptr);

  if (!web_contents_android)
    return ScopedJavaLocalRef<jobject>();

  // Check to make sure this object hasn't been destroyed.
  if (g_allocated_web_contents_androids.Get().find(web_contents_android) ==
      g_allocated_web_contents_androids.Get().end()) {
    return ScopedJavaLocalRef<jobject>();
  }

  return web_contents_android->GetJavaObject();
}

WebContentsAndroid::WebContentsAndroid(WebContentsImpl* web_contents)
    : web_contents_(web_contents),
      navigation_controller_(&(web_contents->GetController())) {
  g_allocated_web_contents_androids.Get().insert(this);
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(env, reinterpret_cast<intptr_t>(this),
                                         navigation_controller_.GetJavaObject())
                 .obj());
}

WebContentsAndroid::~WebContentsAndroid() {
  DCHECK(g_allocated_web_contents_androids.Get().find(this) !=
      g_allocated_web_contents_androids.Get().end());
  g_allocated_web_contents_androids.Get().erase(this);
  offset_tag_mediator_ = nullptr;
  for (auto& observer : destruction_observers_)
    observer.WebContentsAndroidDestroyed(this);
  Java_WebContentsImpl_clearNativePtr(AttachCurrentThread(), obj_);
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

void WebContentsAndroid::CaptureContentAsBitmapForTesting(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcallback) {
  ui::GrabViewSnapshot(
      web_contents_->GetNativeView(), gfx::Rect(web_contents_->GetSize()),
      base::BindOnce(
          &WebContentsAndroid::OnFinishGetContentBitmapForTesting,
          weak_factory_.GetWeakPtr(),
          base::android::ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

void WebContentsAndroid::OnFinishGetContentBitmapForTesting(
    const base::android::JavaRef<jobject>& callback,
    gfx::Image snapshot) {
  const SkBitmap bitmap = snapshot.AsBitmap();
  CHECK(!bitmap.isNull());
  CHECK(!bitmap.empty());
  base::android::RunObjectCallbackAndroid(
      callback,
      gfx::ConvertToJavaBitmap(bitmap, gfx::OomBehavior::kReturnNullOnOom));
}

void WebContentsAndroid::Init() {
  offset_tag_mediator_ = new BrowserControlsOffsetTagMediator(web_contents_);
  offset_tag_mediator_->Initialize();
}

void WebContentsAndroid::ClearNativeReference(JNIEnv* env) {
  return web_contents_->ClearWebContentsAndroid();
}

void WebContentsAndroid::AddDestructionObserver(DestructionObserver* observer) {
  destruction_observers_.AddObserver(observer);
}

void WebContentsAndroid::RemoveDestructionObserver(
    DestructionObserver* observer) {
  destruction_observers_.RemoveObserver(observer);
}

// static
void WebContentsAndroid::ReportDanglingPtrToBrowserContext(
    JNIEnv* env,
    WebContents* web_contents) {
  if (base::android::ScopedJavaLocalRef<jthrowable> java_creator =
          web_contents->GetJavaCreatorLocation()) {
    Java_WebContentsImpl_reportDanglingPtrToBrowserContext(env, java_creator);
  }
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetTopLevelNativeWindow(JNIEnv* env) {
  ui::WindowAndroid* window_android = web_contents_->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return window_android->GetJavaObject();
}

void WebContentsAndroid::SetTopLevelNativeWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& jwindow_android) {
  ui::WindowAndroid* window =
      ui::WindowAndroid::FromJavaWindowAndroid(jwindow_android);
  auto* old_window = web_contents_->GetTopLevelNativeWindow();
  if (window == old_window)
    return;

  auto* view = web_contents_->GetNativeView();
  if (old_window)
    view->RemoveFromParent();
  if (window)
    window->AddChild(view);
}

void WebContentsAndroid::SetViewAndroidDelegate(
    JNIEnv* env,
    const JavaParamRef<jobject>& jview_delegate) {
  ui::ViewAndroid* view_android = web_contents_->GetView()->GetNativeView();
  view_android->SetDelegate(jview_delegate);
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetMainFrame(
    JNIEnv* env) const {
  return web_contents_->GetPrimaryMainFrame()->GetJavaRenderFrameHost();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetFocusedFrame(
    JNIEnv* env) const {
  RenderFrameHostImpl* rfh = web_contents_->GetFocusedFrame();
  if (!rfh)
    return nullptr;
  return rfh->GetJavaRenderFrameHost();
}

bool WebContentsAndroid::IsFocusedElementEditable(JNIEnv* env) {
  return web_contents_->IsFocusedElementEditable();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetRenderFrameHostFromId(
    JNIEnv* env,
    jint render_process_id,
    jint render_frame_id) const {
  RenderFrameHost* rfh =
      RenderFrameHost::FromID(render_process_id, render_frame_id);
  if (!rfh)
    return nullptr;
  return rfh->GetJavaRenderFrameHost();
}

ScopedJavaLocalRef<jobjectArray> WebContentsAndroid::GetAllRenderFrameHosts(
    JNIEnv* env) const {
  std::vector<RenderFrameHost*> frames;
  web_contents_->ForEachRenderFrameHost(
      [&frames](RenderFrameHostImpl* rfh) { frames.push_back(rfh); });
  ScopedJavaLocalRef<jobjectArray> jframes =
      Java_WebContentsImpl_createRenderFrameHostArray(env, frames.size());
  for (size_t i = 0; i < frames.size(); i++) {
    Java_WebContentsImpl_addRenderFrameHostToArray(
        env, jframes, i, frames[i]->GetJavaRenderFrameHost());
  }
  return jframes;
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetTitle(JNIEnv* env) const {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 web_contents_->GetTitle());
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetVisibleURL(
    JNIEnv* env) const {
  return url::GURLAndroid::FromNativeGURL(env, web_contents_->GetVisibleURL());
}

jint WebContentsAndroid::GetVirtualKeyboardMode(JNIEnv* env) const {
  return static_cast<jint>(web_contents_->GetVirtualKeyboardMode());
}

bool WebContentsAndroid::IsLoading(JNIEnv* env) const {
  return web_contents_->IsLoading();
}

bool WebContentsAndroid::ShouldShowLoadingUI(JNIEnv* env) const {
  return web_contents_->ShouldShowLoadingUI();
}

bool WebContentsAndroid::HasUncommittedNavigationInPrimaryMainFrame(
    JNIEnv* env) const {
  return web_contents_->HasUncommittedNavigationInPrimaryMainFrame();
}

void WebContentsAndroid::DispatchBeforeUnload(JNIEnv* env, bool auto_cancel) {
  web_contents_->DispatchBeforeUnload(auto_cancel);
}

void WebContentsAndroid::Stop(JNIEnv* env) {
  web_contents_->Stop();
}

void WebContentsAndroid::Cut(JNIEnv* env) {
  web_contents_->Cut();
}

void WebContentsAndroid::Copy(JNIEnv* env) {
  web_contents_->Copy();
}

void WebContentsAndroid::Paste(JNIEnv* env) {
  web_contents_->Paste();
}

void WebContentsAndroid::PasteAsPlainText(JNIEnv* env) {
  // Paste as if user typed the characters, which should match current style of
  // the caret location.
  web_contents_->PasteAndMatchStyle();
}

void WebContentsAndroid::Replace(JNIEnv* env,
                                 const JavaParamRef<jstring>& jstr) {
  web_contents_->Replace(base::android::ConvertJavaStringToUTF16(env, jstr));
}

void WebContentsAndroid::SelectAll(JNIEnv* env) {
  web_contents_->SelectAll();
}

void WebContentsAndroid::CollapseSelection(JNIEnv* env) {
  web_contents_->CollapseSelection();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetRenderWidgetHostView(
    JNIEnv* env) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva)
    return nullptr;
  return rwhva->GetJavaObject();
}

jint WebContentsAndroid::GetVisibility(JNIEnv* env) {
  return static_cast<jint>(web_contents_->GetVisibility());
}

void WebContentsAndroid::UpdateWebContentsVisibility(JNIEnv* env,
                                                     jint visibility) {
  web_contents_->UpdateWebContentsVisibility(
      static_cast<Visibility>(visibility));
}

RenderWidgetHostViewAndroid*
    WebContentsAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  rwhv = web_contents_->GetRenderWidgetHostView();
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

jint WebContentsAndroid::GetBackgroundColor(JNIEnv* env) {
  return web_contents_->GetBackgroundColor().value_or(SK_ColorTRANSPARENT);
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetLastCommittedURL(
    JNIEnv* env) const {
  return url::GURLAndroid::FromNativeGURL(env,
                                          web_contents_->GetLastCommittedURL());
}

jboolean WebContentsAndroid::IsIncognito(JNIEnv* env) {
  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsAndroid::ResumeLoadingCreatedWebContents(JNIEnv* env) {
  web_contents_->ResumeLoadingCreatedWebContents();
}

void WebContentsAndroid::SetImportance(JNIEnv* env,
                                       jint primary_main_frame_importance) {
  web_contents_->SetPrimaryMainFrameImportance(
      static_cast<ChildProcessImportance>(primary_main_frame_importance));
}

void WebContentsAndroid::SuspendAllMediaPlayers(JNIEnv* env) {
  web_contents_->media_web_contents_observer()->SuspendAllMediaPlayers();
}

void WebContentsAndroid::SetAudioMuted(JNIEnv* env, jboolean mute) {
  web_contents_->SetAudioMuted(mute);
}

jboolean WebContentsAndroid::IsAudioMuted(JNIEnv* env) {
  return web_contents_->IsAudioMuted();
}

jboolean WebContentsAndroid::FocusLocationBarByDefault(JNIEnv* env) {
  return web_contents_->FocusLocationBarByDefault();
}

bool WebContentsAndroid::IsFullscreenForCurrentTab(JNIEnv* env) {
  return web_contents_->IsFullscreen();
}

void WebContentsAndroid::ExitFullscreen(JNIEnv* env) {
  web_contents_->ExitFullscreen(/*will_cause_resize=*/false);
}

void WebContentsAndroid::ScrollFocusedEditableNodeIntoView(JNIEnv* env) {
  auto* input_handler = web_contents_->GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;
  bool should_overlay_content =
      web_contents_->GetPrimaryPage().virtual_keyboard_mode() ==
      ui::mojom::VirtualKeyboardMode::kOverlaysContent;
  // TODO(bokan): Autofill is notified of focus changes at the end of the
  // scrollIntoView call using DidCompleteFocusChangeInFrame, see
  // https://source.chromium.org/chromium/chromium/src/+/main:third_party/blink/renderer/core/frame/web_local_frame_impl.cc;l=3047;drc=aeadb03c8553c39e88d5d11d10f706d42f06a1d7.
  // By avoiding this call in should_overlay_content, we never notify autofill
  // of changed focus so we don't e.g. show the keyboard accessory.
  if (!should_overlay_content)
    input_handler->ScrollFocusedEditableNodeIntoView();
}

void WebContentsAndroid::SelectAroundCaretAck(
    int startOffset,
    int endOffset,
    int surroundingTextLength,
    blink::mojom::SelectAroundCaretResultPtr result) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva) {
    rwhva->SelectAroundCaretAck(startOffset, endOffset, surroundingTextLength,
                                std::move(result));
  }
}

void WebContentsAndroid::SelectAroundCaret(JNIEnv* env,
                                           jint granularity,
                                           jboolean should_show_handle,
                                           jboolean should_show_context_menu,
                                           jint startOffset,
                                           jint endOffset,
                                           jint surroundingTextLength) {
  auto* input_handler = web_contents_->GetFocusedFrameWidgetInputHandler();
  if (!input_handler)
    return;
  input_handler->SelectAroundCaret(
      static_cast<blink::mojom::SelectionGranularity>(granularity),
      should_show_handle, should_show_context_menu,
      base::BindOnce(&WebContentsAndroid::SelectAroundCaretAck,
                     weak_factory_.GetWeakPtr(), startOffset, endOffset,
                     surroundingTextLength));
}

void WebContentsAndroid::AdjustSelectionByCharacterOffset(
    JNIEnv* env,
    jint start_adjust,
    jint end_adjust,
    jboolean show_selection_menu) {
  web_contents_->AdjustSelectionByCharacterOffset(start_adjust, end_adjust,
                                                  show_selection_menu);
}

bool WebContentsAndroid::InitializeRenderFrameForJavaScript() {
  if (!web_contents_->GetPrimaryFrameTree()
           .root()
           ->render_manager()
           ->InitializeMainRenderFrameForImmediateUse()) {
    LOG(ERROR) << "Failed to initialize RenderFrame to evaluate javascript";
    return false;
  }
  return true;
}

void WebContentsAndroid::EvaluateJavaScript(
    JNIEnv* env,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!InitializeRenderFrameForJavaScript())
    return;

  if (!callback) {
    // No callback requested.
    web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(
        ConvertJavaStringToUTF16(env, script), base::NullCallback());
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::OnceCallback below.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScript(
      ConvertJavaStringToUTF16(env, script),
      base::BindOnce(&JavaScriptResultCallback, j_callback));
}

void WebContentsAndroid::EvaluateJavaScriptForTests(
    JNIEnv* env,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!InitializeRenderFrameForJavaScript())
    return;

  if (!callback) {
    // No callback requested.
    web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
        ConvertJavaStringToUTF16(env, script), base::NullCallback(),
        ISOLATED_WORLD_ID_GLOBAL);
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::OnceCallback below.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetPrimaryMainFrame()->ExecuteJavaScriptForTests(
      ConvertJavaStringToUTF16(env, script),
      base::BindOnce(&JavaScriptResultCallback, j_callback),
      ISOLATED_WORLD_ID_GLOBAL);
}

void WebContentsAndroid::AddMessageToDevToolsConsole(
    JNIEnv* env,
    jint level,
    const JavaParamRef<jstring>& message) {
  DCHECK_GE(level, 0);
  DCHECK_LE(level, static_cast<int>(blink::mojom::ConsoleMessageLevel::kError));

  web_contents_->GetPrimaryMainFrame()->AddMessageToConsole(
      static_cast<blink::mojom::ConsoleMessageLevel>(level),
      ConvertJavaStringToUTF8(env, message));
}

void WebContentsAndroid::PostMessageToMainFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& jmessage,
    const JavaParamRef<jstring>& jsource_origin,
    const JavaParamRef<jstring>& jtarget_origin,
    const JavaParamRef<jobjectArray>& jports) {
  content::MessagePortProvider::PostMessageToFrame(
      web_contents_->GetPrimaryPage(), env, jsource_origin, jtarget_origin,
      jmessage, jports);
}

jboolean WebContentsAndroid::HasAccessedInitialDocument(JNIEnv* env) {
  return static_cast<WebContentsImpl*>(web_contents_)->
      HasAccessedInitialDocument();
}

jboolean WebContentsAndroid::HasViewTransitionOptIn(JNIEnv* env) {
  auto* opt_in_state = ViewTransitionOptInState::GetForCurrentDocument(
      web_contents_->GetPrimaryMainFrame());
  return opt_in_state &&
         opt_in_state->same_origin_opt_in() ==
             blink::mojom::ViewTransitionSameOriginOptIn::kEnabled;
}

jint WebContentsAndroid::GetThemeColor(JNIEnv* env) {
  return web_contents_->GetThemeColor().value_or(SK_ColorTRANSPARENT);
}

jfloat WebContentsAndroid::GetLoadProgress(JNIEnv* env) {
  return web_contents_->GetLoadProgress();
}

void WebContentsAndroid::RequestSmartClipExtract(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback,
    jint x,
    jint y,
    jint width,
    jint height) {
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::OnceCallback below.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetPrimaryMainFrame()->RequestSmartClipExtract(
      base::BindOnce(&SmartClipCallback, j_callback),
      gfx::Rect(x, y, width, height));
}

void WebContentsAndroid::AXTreeSnapshotCallback(
    const JavaRef<jobject>& view_structure_root,
    const JavaRef<jobject>& view_structure_builder,
    const JavaRef<jobject>& callback,
    ui::AXTreeUpdate& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (result.nodes.empty()) {
    RunRunnableAndroid(callback);
    return;
  }
  std::unique_ptr<ui::AssistantTree> assistant_tree =
      ui::CreateAssistantTree(result);
  CreateJavaAXSnapshot(env, assistant_tree.get(),
                       assistant_tree->nodes.front().get(), view_structure_root,
                       view_structure_builder, true);
  AddTreeLevelDataToViewStructure(env, view_structure_root,
                                  view_structure_builder, result);
  RunRunnableAndroid(callback);
}

void WebContentsAndroid::RequestAccessibilitySnapshot(
    JNIEnv* env,
    const JavaParamRef<jobject>& view_structure_root,
    const JavaParamRef<jobject>& view_structure_builder,
    const JavaParamRef<jobject>& callback) {
  // Secure the Java objects in scoped objects and give ownership of them to the
  // base::OnceCallback below.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  ScopedJavaGlobalRef<jobject> j_view_structure_root;
  j_view_structure_root.Reset(env, view_structure_root);
  ScopedJavaGlobalRef<jobject> j_view_structure_builder;
  j_view_structure_builder.Reset(env, view_structure_builder);

  // Set a timeout of 2.0 seconds to compute the snapshot of the
  // accessibility tree because Google Assistant ignores results that
  // don't come back within 3.0 seconds.
  // TODO(nektar): Investigate removal of html mode for Android.
  static_cast<WebContentsImpl*>(web_contents_)
      ->RequestAXTreeSnapshot(
          base::BindOnce(
              &WebContentsAndroid::AXTreeSnapshotCallback,
              weak_factory_.GetWeakPtr(), std::move(j_view_structure_root),
              std::move(j_view_structure_builder), std::move(j_callback)),
          ui::AXMode(ui::kAXModeComplete.flags() | ui::AXMode::kHTML |
                     ui::AXMode::kHTMLMetadata),
          /* max_nodes= */ 5000,
          /* timeout= */ base::Seconds(2),
          WebContents::AXTreeSnapshotPolicy::kAll);
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetEncoding(JNIEnv* env) const {
  return base::android::ConvertUTF8ToJavaString(env,
                                                web_contents_->GetEncoding());
}

void WebContentsAndroid::SetOverscrollRefreshHandler(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& overscroll_refresh_handler) {
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->SetOverscrollRefreshHandler(
      std::make_unique<ui::OverscrollRefreshHandler>(
          overscroll_refresh_handler));
}

void WebContentsAndroid::SetSpatialNavigationDisabled(JNIEnv* env,
                                                      bool disabled) {
  web_contents_->SetSpatialNavigationDisabled(disabled);
}

void WebContentsAndroid::SetStylusHandwritingEnabled(JNIEnv* env,
                                                     bool enabled) {
  web_contents_->SetStylusHandwritingEnabled(enabled);
}

int WebContentsAndroid::DownloadImage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jurl,
    jboolean is_fav_icon,
    jint max_bitmap_size,
    jboolean bypass_cache,
    const base::android::JavaParamRef<jobject>& jcallback) {
  const gfx::Size preferred_size;
  return web_contents_->DownloadImage(
      url::GURLAndroid::ToNativeGURL(env, jurl), is_fav_icon, preferred_size,
      max_bitmap_size, bypass_cache,
      base::BindOnce(&WebContentsAndroid::OnFinishDownloadImage,
                     weak_factory_.GetWeakPtr(), obj_,
                     ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

void WebContentsAndroid::SetHasPersistentVideo(JNIEnv* env, jboolean value) {
  web_contents_->SetHasPersistentVideo(value);
}

bool WebContentsAndroid::HasActiveEffectivelyFullscreenVideo(JNIEnv* env) {
  return web_contents_->HasActiveEffectivelyFullscreenVideo();
}

bool WebContentsAndroid::IsPictureInPictureAllowedForFullscreenVideo(
    JNIEnv* env) {
  return web_contents_->IsPictureInPictureAllowedForFullscreenVideo();
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetFullscreenVideoSize(JNIEnv* env) {
  if (!web_contents_->GetFullscreenVideoSize())
    return ScopedJavaLocalRef<jobject>();  // Return null.

  gfx::Size size = web_contents_->GetFullscreenVideoSize().value();
  return Java_WebContentsImpl_createSize(env, size.width(), size.height());
}

void WebContentsAndroid::SetSize(JNIEnv* env, jint width, jint height) {
  web_contents_->GetNativeView()->OnSizeChanged(width, height);
}

int WebContentsAndroid::GetWidth(JNIEnv* env) {
  return web_contents_->GetNativeView()->GetSize().width();
}

int WebContentsAndroid::GetHeight(JNIEnv* env) {
  return web_contents_->GetNativeView()->GetSize().height();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetOrCreateEventForwarder(
    JNIEnv* env) {
  gfx::NativeView native_view = web_contents_->GetView()->GetNativeView();
  return native_view->GetEventForwarder();
}

void WebContentsAndroid::OnFinishDownloadImage(
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& callback,
    int id,
    int http_status_code,
    const GURL& url,
    const std::vector<SkBitmap>& bitmaps,
    const std::vector<gfx::Size>& sizes) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jbitmaps =
      Java_WebContentsImpl_createBitmapList(env);
  ScopedJavaLocalRef<jobject> jsizes =
      Java_WebContentsImpl_createSizeList(env);
  ScopedJavaLocalRef<jobject> jurl = url::GURLAndroid::FromNativeGURL(env, url);

  for (const SkBitmap& bitmap : bitmaps) {
    // WARNING: convering to java bitmaps results in duplicate memory
    // allocations, which increases the chance of OOMs if DownloadImage() is
    // misused.
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(bitmap);
    Java_WebContentsImpl_addToBitmapList(env, jbitmaps, jbitmap);
  }
  for (const gfx::Size& size : sizes) {
    Java_WebContentsImpl_createSizeAndAddToList(env, jsizes, size.width(),
                                                size.height());
  }
  Java_WebContentsImpl_onDownloadImageFinished(
      env, obj, callback, id, http_status_code, jurl, jbitmaps, jsizes);
}

void WebContentsAndroid::SetMediaSession(
    const ScopedJavaLocalRef<jobject>& j_media_session) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_WebContentsImpl_setMediaSession(env, obj_, j_media_session);
}

void WebContentsAndroid::SendOrientationChangeEvent(JNIEnv* env,
                                                    jint orientation) {
  base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->set_device_orientation(orientation);
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva)
    rwhva->UpdateScreenInfo();

  web_contents_->OnScreenOrientationChange();
}

void WebContentsAndroid::OnScaleFactorChanged(JNIEnv* env) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    rwhva->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                       std::nullopt);
  }
}

void WebContentsAndroid::SetFocus(JNIEnv* env, jboolean focused) {
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->SetFocus(focused);
}

bool WebContentsAndroid::IsBeingDestroyed(JNIEnv* env) {
  return web_contents_->IsBeingDestroyed();
}

void WebContentsAndroid::SetDisplayCutoutSafeArea(JNIEnv* env,
                                                  int top,
                                                  int left,
                                                  int bottom,
                                                  int right) {
  web_contents()->SetDisplayCutoutSafeArea(
      gfx::Insets::TLBR(top, left, bottom, right));
}

void WebContentsAndroid::NotifyRendererPreferenceUpdate(JNIEnv* env) {
  web_contents_->OnWebPreferencesChanged();
}

void WebContentsAndroid::NotifyBrowserControlsHeightChanged(JNIEnv* env) {
  web_contents_->GetNativeView()->OnBrowserControlsHeightChanged();
}

bool WebContentsAndroid::NeedToFireBeforeUnloadOrUnloadEvents(JNIEnv* env) {
  return web_contents_->NeedToFireBeforeUnloadOrUnloadEvents();
}

void WebContentsAndroid::OnContentForNavigationEntryShown(JNIEnv* env) {
  if (auto* animation =
          web_contents_->GetBackForwardTransitionAnimationManager()) {
    animation->OnContentForNavigationEntryShown();
  }
}

jint WebContentsAndroid::GetCurrentBackForwardTransitionStage(JNIEnv* env) {
  auto stage = BackForwardTransitionAnimationManager::AnimationStage::kNone;
  if (auto* animation =
          web_contents_->GetBackForwardTransitionAnimationManager()) {
    stage = animation->GetCurrentAnimationStage();
  }
  return static_cast<jint>(stage);
}

void WebContentsAndroid::SetLongPressLinkSelectText(JNIEnv* env,
                                                    jboolean enabled) {
  web_contents_->SetLongPressLinkSelectText((bool)enabled);
}

void WebContentsAndroid::NotifyControlsConstraintsChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jold_tags_info,
    const base::android::JavaParamRef<jobject>& jtags_info) {
  cc::BrowserControlsOffsetTagsInfo tags_info =
      cc::android::FromJavaBrowserControlsOffsetTagsInfo(env, jtags_info);
  if (!offset_tag_mediator_) {
    Init();
  }
  offset_tag_mediator_->SetOffsetTagsInfo(tags_info);
}

WebContentsAndroid::BrowserControlsOffsetTagMediator::
    BrowserControlsOffsetTagMediator(WebContents* web_contents)
    : RenderWidgetHostConnector(web_contents) {}

WebContentsAndroid::BrowserControlsOffsetTagMediator::
    ~BrowserControlsOffsetTagMediator() = default;

void WebContentsAndroid::BrowserControlsOffsetTagMediator::SetOffsetTagsInfo(
    const cc::BrowserControlsOffsetTagsInfo& new_offset_tags_info) {
  if (rwhva_) {
    rwhva_->UnregisterOffsetTags(offset_tags_info_);
    rwhva_->RegisterOffsetTags(new_offset_tags_info);
  }

  offset_tags_info_ = new_offset_tags_info;
}

void WebContentsAndroid::BrowserControlsOffsetTagMediator::
    UpdateRenderProcessConnection(RenderWidgetHostViewAndroid* old_rwhva,
                                  RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva) {
    old_rwhva->UnregisterOffsetTags(offset_tags_info_);
  }
  if (new_rwhva) {
    new_rwhva->RegisterOffsetTags(offset_tags_info_);
  }
  rwhva_ = new_rwhva;
}

}  // namespace content
