// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/web_contents/web_contents_android.h"

#include <stdint.h>
#include <string>
#include <utility>
#include <vector>

#include "base/android/callback_android.h"
#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "base/containers/hash_tables.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/metrics/user_metrics.h"
#include "base/task/post_task.h"
#include "base/threading/scoped_blocking_call.h"
#include "content/browser/accessibility/browser_accessibility_android.h"
#include "content/browser/accessibility/browser_accessibility_manager_android.h"
#include "content/browser/android/java/gin_java_bridge_dispatcher_host.h"
#include "content/browser/frame_host/interstitial_page_impl.h"
#include "content/browser/media/android/browser_media_player_manager.h"
#include "content/browser/media/android/media_web_contents_observer_android.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "content/common/frame.mojom.h"
#include "content/common/frame_messages.h"
#include "content/common/input_messages.h"
#include "content/common/view_messages.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_provider.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "jni/WebContentsImpl_jni.h"
#include "net/android/network_library.h"
#include "ui/accessibility/ax_assistant_structure.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_assistant_structure.mojom.h"
#include "ui/android/overscroll_refresh_handler.h"
#include "ui/android/window_android.h"
#include "ui/gfx/android/java_bitmap.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF8ToJavaString;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaIntArray;

namespace content {

namespace {

// Track all WebContentsAndroid objects here so that we don't deserialize a
// destroyed WebContents object.
base::LazyInstance<base::hash_set<WebContentsAndroid*>>::Leaky
    g_allocated_web_contents_androids = LAZY_INSTANCE_INITIALIZER;

void JavaScriptResultCallback(const ScopedJavaGlobalRef<jobject>& callback,
                              const base::Value* result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  std::string json;
  base::JSONWriter::Write(*result, &json);
  ScopedJavaLocalRef<jstring> j_json = ConvertUTF8ToJavaString(env, json);
  Java_WebContentsImpl_onEvaluateJavaScriptResult(env, j_json, callback);
}

void SmartClipCallback(const ScopedJavaGlobalRef<jobject>& callback,
                       const base::string16& text,
                       const base::string16& html,
                       const gfx::Rect& clip_rect) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_text = ConvertUTF16ToJavaString(env, text);
  ScopedJavaLocalRef<jstring> j_html = ConvertUTF16ToJavaString(env, html);
  Java_WebContentsImpl_onSmartClipDataExtracted(
      env, j_text, j_html, clip_rect.x(), clip_rect.y(), clip_rect.right(),
      clip_rect.bottom(), callback);
}

ScopedJavaLocalRef<jobject> JNI_WebContentsImpl_CreateJavaAXSnapshot(
    JNIEnv* env,
    const ui::AssistantTree* tree,
    const ui::AssistantNode* node,
    bool is_root) {
  ScopedJavaLocalRef<jstring> j_text =
      ConvertUTF16ToJavaString(env, node->text);
  ScopedJavaLocalRef<jstring> j_class =
      ConvertUTF8ToJavaString(env, node->class_name);
  ScopedJavaLocalRef<jobject> j_node =
      Java_WebContentsImpl_createAccessibilitySnapshotNode(
          env, node->rect.x(), node->rect.y(), node->rect.width(),
          node->rect.height(), is_root, j_text, node->color, node->bgcolor,
          node->text_size, node->bold, node->italic, node->underline,
          node->line_through, j_class);

  if (node->selection.has_value()) {
    Java_WebContentsImpl_setAccessibilitySnapshotSelection(
        env, j_node, node->selection->start(), node->selection->end());
  }

  for (int child : node->children_indices) {
    Java_WebContentsImpl_addAccessibilityNodeAsChild(
        env, j_node,
        JNI_WebContentsImpl_CreateJavaAXSnapshot(
            env, tree, tree->nodes[child].get(), false));
  }
  return j_node;
}

// Walks over the AXTreeUpdate and creates a light weight snapshot.
void AXTreeSnapshotCallback(const ScopedJavaGlobalRef<jobject>& callback,
                            const ui::AXTreeUpdate& result) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (result.nodes.empty()) {
    Java_WebContentsImpl_onAccessibilitySnapshot(env, nullptr, callback);
    return;
  }
  std::unique_ptr<BrowserAccessibilityManagerAndroid> manager(
      static_cast<BrowserAccessibilityManagerAndroid*>(
          BrowserAccessibilityManager::Create(result, nullptr)));
  std::unique_ptr<ui::AssistantTree> assistant_tree =
      ui::CreateAssistantTree(result, manager->ShouldExposePasswordText());
  ScopedJavaLocalRef<jobject> j_root = JNI_WebContentsImpl_CreateJavaAXSnapshot(
      env, assistant_tree.get(), assistant_tree->nodes.front().get(), true);
  Java_WebContentsImpl_onAccessibilitySnapshot(env, j_root, callback);
}

std::string CompressAndSaveBitmap(const std::string& dir,
                                  const SkBitmap& bitmap) {
  base::ScopedBlockingCall scoped_blocking_call(base::BlockingType::WILL_BLOCK);

  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(bitmap, 85, &data)) {
    LOG(ERROR) << "Failed to encode bitmap to JPEG";
    return std::string();
  }

  base::FilePath screenshot_dir(dir);
  if (!base::DirectoryExists(screenshot_dir)) {
    if (!base::CreateDirectory(screenshot_dir)) {
      LOG(ERROR) << "Failed to create screenshot directory";
      return std::string();
    }
  }

  base::FilePath screenshot_path;
  base::ScopedFILE out_file(
      base::CreateAndOpenTemporaryFileInDir(screenshot_dir, &screenshot_path));
  if (!out_file) {
    LOG(ERROR) << "Failed to create temporary screenshot file";
    return std::string();
  }
  unsigned int bytes_written =
      fwrite(reinterpret_cast<const char*>(data.data()), 1, data.size(),
             out_file.get());

  // If there were errors, don't leave a partial file around.
  if (bytes_written != data.size()) {
    base::DeleteFile(screenshot_path, false);
    LOG(ERROR) << "Error writing screenshot file to disk";
    return std::string();
  }
  return screenshot_path.value();
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
    const JavaParamRef<jclass>& clazz,
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
    const JavaParamRef<jclass>& clazz,
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
      navigation_controller_(&(web_contents->GetController())),
      weak_factory_(this) {
  g_allocated_web_contents_androids.Get().insert(this);
  JNIEnv* env = AttachCurrentThread();
  obj_.Reset(env,
             Java_WebContentsImpl_create(env, reinterpret_cast<intptr_t>(this),
                                         navigation_controller_.GetJavaObject())
                 .obj());
  RendererPreferences* prefs = web_contents_->GetMutableRendererPrefs();
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  prefs->network_contry_iso =
      command_line->HasSwitch(switches::kNetworkCountryIso) ?
          command_line->GetSwitchValueASCII(switches::kNetworkCountryIso)
          : net::android::GetTelephonyNetworkCountryIso();
}

WebContentsAndroid::~WebContentsAndroid() {
  DCHECK(g_allocated_web_contents_androids.Get().find(this) !=
      g_allocated_web_contents_androids.Get().end());
  g_allocated_web_contents_androids.Get().erase(this);
  Java_WebContentsImpl_clearNativePtr(AttachCurrentThread(), obj_);
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(obj_);
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetTopLevelNativeWindow(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  ui::WindowAndroid* window_android = web_contents_->GetTopLevelNativeWindow();
  if (!window_android)
    return nullptr;
  return window_android->GetJavaObject();
}

void WebContentsAndroid::SetTopLevelNativeWindow(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
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
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& jview_delegate) {
  ui::ViewAndroid* view_android = web_contents_->GetView()->GetNativeView();
  view_android->SetDelegate(jview_delegate);
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetMainFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return web_contents_->GetMainFrame()->GetJavaRenderFrameHost();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetTitle(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF16ToJavaString(env,
                                                 web_contents_->GetTitle());
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetVisibleURL(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF8ToJavaString(
      env, web_contents_->GetVisibleURL().spec());
}

bool WebContentsAndroid::IsLoading(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) const {
  return web_contents_->IsLoading();
}

bool WebContentsAndroid::IsLoadingToDifferentDocument(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return web_contents_->IsLoadingToDifferentDocument();
}

void WebContentsAndroid::Stop(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Stop();
}

void WebContentsAndroid::Cut(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Cut();
}

void WebContentsAndroid::Copy(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Copy();
}

void WebContentsAndroid::Paste(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->Paste();
}

void WebContentsAndroid::PasteAsPlainText(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  // Paste as if user typed the characters, which should match current style of
  // the caret location.
  web_contents_->PasteAndMatchStyle();
}

void WebContentsAndroid::Replace(JNIEnv* env,
                                 const JavaParamRef<jobject>& obj,
                                 const JavaParamRef<jstring>& jstr) {
  web_contents_->Replace(base::android::ConvertJavaStringToUTF16(env, jstr));
}

void WebContentsAndroid::SelectAll(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj) {
  web_contents_->SelectAll();
}

void WebContentsAndroid::CollapseSelection(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj) {
  web_contents_->CollapseSelection();
}

RenderWidgetHostViewAndroid*
    WebContentsAndroid::GetRenderWidgetHostViewAndroid() {
  RenderWidgetHostView* rwhv = NULL;
  rwhv = web_contents_->GetRenderWidgetHostView();
  if (web_contents_->ShowingInterstitialPage()) {
    rwhv = web_contents_->GetInterstitialPage()
               ->GetMainFrame()
               ->GetRenderViewHost()
               ->GetWidget()
               ->GetView();
  }
  return static_cast<RenderWidgetHostViewAndroid*>(rwhv);
}

jint WebContentsAndroid::GetBackgroundColor(JNIEnv* env,
                                            const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (!rwhva || !rwhva->GetCachedBackgroundColor())
    return SK_ColorWHITE;
  return *rwhva->GetCachedBackgroundColor();
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetLastCommittedURL(
    JNIEnv* env,
    const JavaParamRef<jobject>&) const {
  return ConvertUTF8ToJavaString(env,
                                 web_contents_->GetLastCommittedURL().spec());
}

jboolean WebContentsAndroid::IsIncognito(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj) {
  return web_contents_->GetBrowserContext()->IsOffTheRecord();
}

void WebContentsAndroid::ResumeLoadingCreatedWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  web_contents_->ResumeLoadingCreatedWebContents();
}

void WebContentsAndroid::OnHide(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->WasHidden();
}

void WebContentsAndroid::OnShow(JNIEnv* env, const JavaParamRef<jobject>& obj) {
  web_contents_->WasShown();
}

void WebContentsAndroid::SetImportance(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint main_frame_importance) {
  web_contents_->SetMainFrameImportance(
      static_cast<ChildProcessImportance>(main_frame_importance));
}

void WebContentsAndroid::SuspendAllMediaPlayers(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  MediaWebContentsObserverAndroid::FromWebContents(web_contents_)
      ->SuspendAllMediaPlayers();
}

void WebContentsAndroid::SetAudioMuted(JNIEnv* env,
                                       const JavaParamRef<jobject>& jobj,
                                       jboolean mute) {
  web_contents_->SetAudioMuted(mute);
}

jboolean WebContentsAndroid::IsShowingInterstitialPage(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->ShowingInterstitialPage();
}

jboolean WebContentsAndroid::FocusLocationBarByDefault(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  return web_contents_->FocusLocationBarByDefault();
}

jboolean WebContentsAndroid::IsRenderWidgetHostViewReady(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  return view && view->HasValidFrame();
}

void WebContentsAndroid::ExitFullscreen(JNIEnv* env,
                                        const JavaParamRef<jobject>& obj) {
  web_contents_->ExitFullscreen(/*will_cause_resize=*/false);
}

void WebContentsAndroid::ScrollFocusedEditableNodeIntoView(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderFrameHostImpl* frame = web_contents_->GetFocusedFrame();
  if (!frame)
    return;
  frame->GetFrameInputHandler()->ScrollFocusedEditableNodeIntoRect(gfx::Rect());
}

void WebContentsAndroid::SelectWordAroundCaretAck(bool did_select,
                                                  int start_adjust,
                                                  int end_adjust) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva)
    rwhva->SelectWordAroundCaretAck(did_select, start_adjust, end_adjust);
}

void WebContentsAndroid::SelectWordAroundCaret(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderFrameHostImpl* frame = web_contents_->GetFocusedFrame();
  if (!frame)
    return;
  frame->GetFrameInputHandler()->SelectWordAroundCaret(
      base::BindOnce(&WebContentsAndroid::SelectWordAroundCaretAck,
                     weak_factory_.GetWeakPtr()));
}

void WebContentsAndroid::AdjustSelectionByCharacterOffset(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint start_adjust,
    jint end_adjust,
    jboolean show_selection_menu) {
  web_contents_->AdjustSelectionByCharacterOffset(start_adjust, end_adjust,
                                                  show_selection_menu);
}

void WebContentsAndroid::EvaluateJavaScript(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents_)->
        CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR) << "Failed to create RenderView in EvaluateJavaScript";
      return;
    }
  }

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScript(
        ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  RenderFrameHost::JavaScriptResultCallback js_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  web_contents_->GetMainFrame()->ExecuteJavaScript(
      ConvertJavaStringToUTF16(env, script), js_callback);
}

void WebContentsAndroid::EvaluateJavaScriptForTests(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& script,
    const JavaParamRef<jobject>& callback) {
  RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);

  if (!rvh->IsRenderViewLive()) {
    if (!static_cast<WebContentsImpl*>(web_contents_)->
        CreateRenderViewForInitialEmptyDocument()) {
      LOG(ERROR) << "Failed to create RenderView in EvaluateJavaScriptForTests";
      return;
    }
  }

  if (!callback) {
    // No callback requested.
    web_contents_->GetMainFrame()->ExecuteJavaScriptForTests(
        ConvertJavaStringToUTF16(env, script));
    return;
  }

  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);
  RenderFrameHost::JavaScriptResultCallback js_callback =
      base::Bind(&JavaScriptResultCallback, j_callback);

  web_contents_->GetMainFrame()->ExecuteJavaScriptForTests(
      ConvertJavaStringToUTF16(env, script), js_callback);
}

void WebContentsAndroid::AddMessageToDevToolsConsole(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj,
    jint level,
    const JavaParamRef<jstring>& message) {
  DCHECK_GE(level, 0);
  DCHECK_LE(level, CONSOLE_MESSAGE_LEVEL_LAST);

  web_contents_->GetMainFrame()->AddMessageToConsole(
      static_cast<ConsoleMessageLevel>(level),
      ConvertJavaStringToUTF8(env, message));
}

void WebContentsAndroid::PostMessageToFrame(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jstring>& jframe_name,
    const JavaParamRef<jstring>& jmessage,
    const JavaParamRef<jstring>& jsource_origin,
    const JavaParamRef<jstring>& jtarget_origin,
    const JavaParamRef<jobjectArray>& jports) {
  content::MessagePortProvider::PostMessageToFrame(
      web_contents_, env, jsource_origin, jtarget_origin, jmessage, jports);
}

jboolean WebContentsAndroid::HasAccessedInitialDocument(
    JNIEnv* env,
    const JavaParamRef<jobject>& jobj) {
  return static_cast<WebContentsImpl*>(web_contents_)->
      HasAccessedInitialDocument();
}

jint WebContentsAndroid::GetThemeColor(JNIEnv* env,
                                       const JavaParamRef<jobject>& obj) {
  return web_contents_->GetThemeColor();
}

void WebContentsAndroid::RequestSmartClipExtract(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback,
    jint x,
    jint y,
    jint width,
    jint height) {
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  web_contents_->GetMainFrame()->RequestSmartClipExtract(
      base::BindOnce(&SmartClipCallback, j_callback),
      gfx::Rect(x, y, width, height));
}

void WebContentsAndroid::RequestAccessibilitySnapshot(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    const JavaParamRef<jobject>& callback) {
  // Secure the Java callback in a scoped object and give ownership of it to the
  // base::Callback.
  ScopedJavaGlobalRef<jobject> j_callback;
  j_callback.Reset(env, callback);

  static_cast<WebContentsImpl*>(web_contents_)
      ->RequestAXTreeSnapshot(
          base::BindOnce(&AXTreeSnapshotCallback, j_callback),
          ui::kAXModeComplete);
}

ScopedJavaLocalRef<jstring> WebContentsAndroid::GetEncoding(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) const {
  return base::android::ConvertUTF8ToJavaString(env,
                                                web_contents_->GetEncoding());
}

void WebContentsAndroid::SetOverscrollRefreshHandler(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& overscroll_refresh_handler) {
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->SetOverscrollRefreshHandler(
      std::make_unique<ui::OverscrollRefreshHandler>(
          overscroll_refresh_handler));
}

void WebContentsAndroid::WriteContentBitmapToDisk(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint width,
    jint height,
    const JavaParamRef<jstring>& jpath,
    const JavaParamRef<jobject>& jcallback) {
  base::OnceCallback<void(const SkBitmap&)> result_callback = base::BindOnce(
      &WebContentsAndroid::OnFinishGetContentBitmap, weak_factory_.GetWeakPtr(),
      ScopedJavaGlobalRef<jobject>(env, obj),
      ScopedJavaGlobalRef<jobject>(env, jcallback),
      ConvertJavaStringToUTF8(env, jpath));
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (!view) {
    std::move(result_callback).Run(SkBitmap());
    return;
  }
  view->CopyFromSurface(gfx::Rect(), gfx::Size(width, height),
                        std::move(result_callback));
}

void WebContentsAndroid::ReloadLoFiImages(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  static_cast<WebContentsImpl*>(web_contents_)->ReloadLoFiImages();
}

int WebContentsAndroid::DownloadImage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jstring>& jurl,
    jboolean is_fav_icon,
    jint max_bitmap_size,
    jboolean bypass_cache,
    const base::android::JavaParamRef<jobject>& jcallback) {
  GURL url(base::android::ConvertJavaStringToUTF8(env, jurl));
  return web_contents_->DownloadImage(
      url, is_fav_icon, max_bitmap_size, bypass_cache,
      base::Bind(&WebContentsAndroid::OnFinishDownloadImage,
                 weak_factory_.GetWeakPtr(),
                 ScopedJavaGlobalRef<jobject>(env, obj),
                 ScopedJavaGlobalRef<jobject>(env, jcallback)));
}

void WebContentsAndroid::DismissTextHandles(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view)
    view->DismissTextHandles();
}

void WebContentsAndroid::ShowContextMenuAtTouchHandle(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int x,
    int y) {
  RenderWidgetHostViewAndroid* view = GetRenderWidgetHostViewAndroid();
  if (view)
    view->ShowContextMenuAtPoint(gfx::Point(x, y),
                                 ui::MENU_SOURCE_TOUCH_HANDLE);
}

void WebContentsAndroid::SetHasPersistentVideo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jboolean value) {
  web_contents_->SetHasPersistentVideo(value);
}

bool WebContentsAndroid::HasActiveEffectivelyFullscreenVideo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->HasActiveEffectivelyFullscreenVideo();
}

bool WebContentsAndroid::IsPictureInPictureAllowedForFullscreenVideo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->IsPictureInPictureAllowedForFullscreenVideo();
}

base::android::ScopedJavaLocalRef<jobject>
WebContentsAndroid::GetFullscreenVideoSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  if (!web_contents_->GetFullscreenVideoSize())
    return ScopedJavaLocalRef<jobject>();  // Return null.

  gfx::Size size = web_contents_->GetFullscreenVideoSize().value();
  return Java_WebContentsImpl_createSize(env, size.width(), size.height());
}

void WebContentsAndroid::SetSize(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    jint width,
    jint height) {
  web_contents_->GetNativeView()->OnSizeChanged(width, height);
}

int WebContentsAndroid::GetWidth(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->GetNativeView()->GetSize().width();
}

int WebContentsAndroid::GetHeight(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  return web_contents_->GetNativeView()->GetSize().height();
}

ScopedJavaLocalRef<jobject> WebContentsAndroid::GetOrCreateEventForwarder(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj) {
  gfx::NativeView native_view = web_contents_->GetView()->GetNativeView();
  return native_view->GetEventForwarder();
}

void WebContentsAndroid::OnFinishGetContentBitmap(
    const JavaRef<jobject>& obj,
    const JavaRef<jobject>& callback,
    const std::string& path,
    const SkBitmap& bitmap) {
  JNIEnv* env = base::android::AttachCurrentThread();
  if (!bitmap.drawsNothing()) {
    auto task_runner = base::CreateSequencedTaskRunnerWithTraits(
        {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});
    base::PostTaskAndReplyWithResult(
        task_runner.get(), FROM_HERE,
        base::BindOnce(&CompressAndSaveBitmap, path, bitmap),
        base::BindOnce(&base::android::RunStringCallbackAndroid,
                       ScopedJavaGlobalRef<jobject>(env, callback.obj())));
    return;
  }
  // If readback failed, call empty callback
  base::android::RunStringCallbackAndroid(callback, std::string());
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
  ScopedJavaLocalRef<jstring> jurl =
      base::android::ConvertUTF8ToJavaString(env, url.spec());

  for (const SkBitmap& bitmap : bitmaps) {
    // WARNING: convering to java bitmaps results in duplicate memory
    // allocations, which increases the chance of OOMs if DownloadImage() is
    // misused.
    ScopedJavaLocalRef<jobject> jbitmap = gfx::ConvertToJavaBitmap(&bitmap);
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

void WebContentsAndroid::SendOrientationChangeEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj,
    jint orientation) {
  base::RecordAction(base::UserMetricsAction("ScreenOrientationChange"));
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->set_device_orientation(orientation);
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva)
    rwhva->UpdateScreenInfo(web_contents_->GetView()->GetNativeView());

  web_contents_->OnScreenOrientationChange();
}

void WebContentsAndroid::OnScaleFactorChanged(
    JNIEnv* env,
    const JavaParamRef<jobject>& obj) {
  RenderWidgetHostViewAndroid* rwhva = GetRenderWidgetHostViewAndroid();
  if (rwhva) {
    // |SendScreenRects()| indirectly calls GetViewSize() that asks Java layer.
    web_contents_->SendScreenRects();
    rwhva->SynchronizeVisualProperties(cc::DeadlinePolicy::UseDefaultDeadline(),
                                       base::nullopt, base::nullopt);
  }
}

void WebContentsAndroid::SetFocus(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jboolean focused) {
  WebContentsViewAndroid* view =
      static_cast<WebContentsViewAndroid*>(web_contents_->GetView());
  view->SetFocus(focused);
}

bool WebContentsAndroid::IsBeingDestroyed(JNIEnv* env,
                                          const JavaParamRef<jobject>& obj) {
  return web_contents_->IsBeingDestroyed();
}

void WebContentsAndroid::SetDisplayCutoutSafeArea(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    int top,
    int left,
    int bottom,
    int right) {
  web_contents()->SetDisplayCutoutSafeArea(
      gfx::Insets(top, left, bottom, right));
}

}  // namespace content
