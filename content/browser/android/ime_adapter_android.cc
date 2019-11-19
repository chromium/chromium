// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/ime_adapter_android.h"

#include <android/input.h>
#include <algorithm>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "content/browser/android/text_suggestion_host_android.h"
#include "content/browser/frame_host/render_frame_host_impl.h"
#include "content/browser/renderer_host/render_view_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/common/frame_messages.h"
#include "content/common/view_messages.h"
#include "content/public/android/content_jni_headers/ImeAdapterImpl_jni.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "ui/base/ime/ime_text_span.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF16;
using base::android::ConvertUTF16ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {
namespace {

// Maps a java KeyEvent into a NativeWebKeyboardEvent.
// |java_key_event| is used to maintain a globalref for KeyEvent.
// |type| will determine the WebInputEvent type.
// type, |modifiers|, |time_ms|, |key_code|, |unicode_char| is used to create
// WebKeyboardEvent. |key_code| is also needed ad need to treat the enter key
// as a key press of character \r.
NativeWebKeyboardEvent NativeWebKeyboardEventFromKeyEvent(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& java_key_event,
    int type,
    int modifiers,
    jlong time_ms,
    int key_code,
    int scan_code,
    bool is_system_key,
    int unicode_char) {
  return NativeWebKeyboardEvent(
      env, java_key_event, static_cast<blink::WebInputEvent::Type>(type),
      modifiers, base::TimeTicks() + base::TimeDelta::FromMilliseconds(time_ms),
      key_code, scan_code, unicode_char, is_system_key);
}

}  // anonymous namespace

jlong JNI_ImeAdapterImpl_Init(JNIEnv* env,
                              const JavaParamRef<jobject>& obj,
                              const JavaParamRef<jobject>& jweb_contents) {
  WebContents* web_contents = WebContents::FromJavaWebContents(jweb_contents);
  DCHECK(web_contents);
  auto* ime_adapter = new ImeAdapterAndroid(env, obj, web_contents);
  ime_adapter->Initialize();
  return reinterpret_cast<intptr_t>(ime_adapter);
}

// Callback from Java to convert BackgroundColorSpan data to a
// ui::ImeTextSpan instance, and append it to |ime_text_spans_ptr|.
void JNI_ImeAdapterImpl_AppendBackgroundColorSpan(JNIEnv*,
                                                  jlong ime_text_spans_ptr,
                                                  jint start,
                                                  jint end,
                                                  jint background_color) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  // Do not check |background_color|.
  std::vector<ui::ImeTextSpan>* ime_text_spans =
      reinterpret_cast<std::vector<ui::ImeTextSpan>*>(ime_text_spans_ptr);
  ime_text_spans->push_back(ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, static_cast<unsigned>(start),
      static_cast<unsigned>(end), ui::ImeTextSpan::Thickness::kNone,
      static_cast<unsigned>(background_color), SK_ColorTRANSPARENT,
      std::vector<std::string>()));
}

// Callback from Java to convert SuggestionSpan data to a
// ui::ImeTextSpan instance, and append it to |ime_text_spans_ptr|.
void JNI_ImeAdapterImpl_AppendSuggestionSpan(
    JNIEnv* env,
    jlong ime_text_spans_ptr,
    jint start,
    jint end,
    jboolean is_misspelling,
    jboolean remove_on_finish_composing,
    jint underline_color,
    jint suggestion_highlight_color,
    const JavaParamRef<jobjectArray>& suggestions) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);

  ui::ImeTextSpan::Type type =
      is_misspelling ? ui::ImeTextSpan::Type::kMisspellingSuggestion
                     : ui::ImeTextSpan::Type::kSuggestion;

  std::vector<ui::ImeTextSpan>* ime_text_spans =
      reinterpret_cast<std::vector<ui::ImeTextSpan>*>(ime_text_spans_ptr);
  std::vector<std::string> suggestions_vec;
  AppendJavaStringArrayToStringVector(env, suggestions, &suggestions_vec);
  ui::ImeTextSpan ime_text_span = ui::ImeTextSpan(
      type, static_cast<unsigned>(start), static_cast<unsigned>(end),
      ui::ImeTextSpan::Thickness::kThick, SK_ColorTRANSPARENT,
      static_cast<unsigned>(suggestion_highlight_color), suggestions_vec);
  ime_text_span.remove_on_finish_composing = remove_on_finish_composing;
  ime_text_span.underline_color = static_cast<unsigned>(underline_color);
  ime_text_spans->push_back(ime_text_span);
}

// Callback from Java to convert UnderlineSpan data to a
// ui::ImeTextSpan instance, and append it to |ime_text_spans_ptr|.
void JNI_ImeAdapterImpl_AppendUnderlineSpan(JNIEnv*,
                                            jlong ime_text_spans_ptr,
                                            jint start,
                                            jint end) {
  DCHECK_GE(start, 0);
  DCHECK_GE(end, 0);
  std::vector<ui::ImeTextSpan>* ime_text_spans =
      reinterpret_cast<std::vector<ui::ImeTextSpan>*>(ime_text_spans_ptr);
  ime_text_spans->push_back(ui::ImeTextSpan(
      ui::ImeTextSpan::Type::kComposition, static_cast<unsigned>(start),
      static_cast<unsigned>(end), ui::ImeTextSpan::Thickness::kThin,
      SK_ColorTRANSPARENT, SK_ColorTRANSPARENT, std::vector<std::string>()));
}

ImeAdapterAndroid::ImeAdapterAndroid(JNIEnv* env,
                                     const JavaParamRef<jobject>& obj,
                                     WebContents* web_contents)
    : RenderWidgetHostConnector(web_contents), rwhva_(nullptr) {
  java_ime_adapter_ = JavaObjectWeakGlobalRef(env, obj);

  // Set up mojo client for TextSuggestionHost in advance. Java side is
  // initialized lazily right before showing the menu first time.
  TextSuggestionHostAndroid::Create(env, web_contents);
}

ImeAdapterAndroid::~ImeAdapterAndroid() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (!obj.is_null())
    Java_ImeAdapterImpl_onNativeDestroyed(env, obj);
}

void ImeAdapterAndroid::UpdateRenderProcessConnection(
    RenderWidgetHostViewAndroid* old_rwhva,
    RenderWidgetHostViewAndroid* new_rwhva) {
  if (old_rwhva)
    old_rwhva->set_ime_adapter(nullptr);
  if (new_rwhva) {
    new_rwhva->set_ime_adapter(this);
    if (!old_rwhva && new_rwhva) {
      JNIEnv* env = AttachCurrentThread();
      ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
      if (!obj.is_null())
        Java_ImeAdapterImpl_onConnectedToRenderProcess(env, obj);
    }
  }
  rwhva_ = new_rwhva;
}

void ImeAdapterAndroid::UpdateState(const TextInputState& state) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (obj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jstring_text =
      ConvertUTF16ToJavaString(env, state.value);
  Java_ImeAdapterImpl_updateState(
      env, obj, static_cast<int>(state.type), state.flags, state.mode,
      static_cast<int>(state.action), state.show_ime_if_needed,
      state.always_hide_ime, jstring_text, state.selection_start,
      state.selection_end, state.composition_start, state.composition_end,
      state.reply_to_request);
}

void ImeAdapterAndroid::UpdateOnTouchDown() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (obj.is_null())
    return;
  Java_ImeAdapterImpl_updateOnTouchDown(env, obj);
}

void ImeAdapterAndroid::UpdateFrameInfo(
    const gfx::SelectionBound& selection_start,
    float dip_scale,
    float content_offset_ypix) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (obj.is_null())
    return;

  // The CursorAnchorInfo API in Android only supports zero width selection
  // bounds.
  const jboolean has_insertion_marker =
      selection_start.type() == gfx::SelectionBound::CENTER;
  const jboolean is_insertion_marker_visible = selection_start.visible();
  const jfloat insertion_marker_horizontal =
      has_insertion_marker ? selection_start.edge_start().x() : 0.0f;
  const jfloat insertion_marker_top =
      has_insertion_marker ? selection_start.edge_start().y() : 0.0f;
  const jfloat insertion_marker_bottom =
      has_insertion_marker ? selection_start.edge_end().y() : 0.0f;

  Java_ImeAdapterImpl_updateFrameInfo(
      env, obj, dip_scale, content_offset_ypix, has_insertion_marker,
      is_insertion_marker_visible, insertion_marker_horizontal,
      insertion_marker_top, insertion_marker_bottom);
}

void ImeAdapterAndroid::OnRenderFrameMetadataChangedAfterActivation(
    const gfx::SizeF& new_viewport_size) {
  if (old_viewport_size_ == new_viewport_size)
    return;

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (obj.is_null())
    return;

  const jboolean surface_height_reduced =
      new_viewport_size.width() == old_viewport_size_.width() &&
      new_viewport_size.height() < old_viewport_size_.height();
  old_viewport_size_ = new_viewport_size;
  Java_ImeAdapterImpl_onResizeScrollableViewport(env, obj,
                                                 surface_height_reduced);
}

bool ImeAdapterAndroid::SendKeyEvent(
    JNIEnv* env,
    const JavaParamRef<jobject>&,
    const JavaParamRef<jobject>& original_key_event,
    int type,
    int modifiers,
    jlong time_ms,
    int key_code,
    int scan_code,
    bool is_system_key,
    int unicode_char) {
  if (!rwhva_)
    return false;
  NativeWebKeyboardEvent event = NativeWebKeyboardEventFromKeyEvent(
      env, original_key_event, type, modifiers, time_ms, key_code, scan_code,
      is_system_key, unicode_char);
  rwhva_->SendKeyEvent(event);
  return true;
}

void ImeAdapterAndroid::SetComposingText(JNIEnv* env,
                                         const JavaParamRef<jobject>& obj,
                                         const JavaParamRef<jobject>& text,
                                         const JavaParamRef<jstring>& text_str,
                                         int relative_cursor_pos) {
  RenderWidgetHostImpl* rwhi = GetFocusedWidget();
  if (!rwhi)
    return;

  base::string16 text16 = ConvertJavaStringToUTF16(env, text_str);

  std::vector<ui::ImeTextSpan> ime_text_spans =
      GetImeTextSpansFromJava(env, obj, text, text16);

  // Default to plain underline if we didn't find any span that we care about.
  if (ime_text_spans.empty()) {
    ime_text_spans.push_back(
        ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, text16.length(),
                        ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT,
                        SK_ColorTRANSPARENT, std::vector<std::string>()));
  }

  // relative_cursor_pos is as described in the Android API for
  // InputConnection#setComposingText, whereas the parameters for
  // ImeSetComposition are relative to the start of the composition.
  if (relative_cursor_pos > 0)
    relative_cursor_pos = text16.length() + relative_cursor_pos - 1;

  rwhi->ImeSetComposition(text16, ime_text_spans, gfx::Range::InvalidRange(),
                          relative_cursor_pos, relative_cursor_pos);
}

void ImeAdapterAndroid::CommitText(JNIEnv* env,
                                   const JavaParamRef<jobject>& obj,
                                   const JavaParamRef<jobject>& text,
                                   const JavaParamRef<jstring>& text_str,
                                   int relative_cursor_pos) {
  RenderWidgetHostImpl* rwhi = GetFocusedWidget();
  if (!rwhi)
    return;

  base::string16 text16 = ConvertJavaStringToUTF16(env, text_str);

  std::vector<ui::ImeTextSpan> ime_text_spans =
      GetImeTextSpansFromJava(env, obj, text, text16);

  // relative_cursor_pos is as described in the Android API for
  // InputConnection#commitText, whereas the parameters for
  // ImeConfirmComposition are relative to the end of the composition.
  if (relative_cursor_pos > 0)
    relative_cursor_pos--;
  else
    relative_cursor_pos -= text16.length();

  rwhi->ImeCommitText(text16, ime_text_spans, gfx::Range::InvalidRange(),
                      relative_cursor_pos);
}

void ImeAdapterAndroid::FinishComposingText(JNIEnv* env,
                                            const JavaParamRef<jobject>&) {
  RenderWidgetHostImpl* rwhi = GetFocusedWidget();
  if (!rwhi)
    return;

  rwhi->ImeFinishComposingText(true);
}

void ImeAdapterAndroid::CancelComposition() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (!obj.is_null())
    Java_ImeAdapterImpl_cancelComposition(env, obj);
}

void ImeAdapterAndroid::FocusedNodeChanged(bool is_editable_node) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (!obj.is_null()) {
    Java_ImeAdapterImpl_focusedNodeChanged(env, obj, is_editable_node);
  }
}

void ImeAdapterAndroid::AdvanceFocusInForm(JNIEnv* env,
                                           const JavaParamRef<jobject>& obj,
                                           jint focus_type) {
  RenderFrameHostImpl* rfh =
      static_cast<RenderFrameHostImpl*>(GetFocusedFrame());
  if (!rfh)
    return;

  rfh->Send(new FrameMsg_AdvanceFocusInForm(
      rfh->GetRoutingID(), static_cast<blink::WebFocusType>(focus_type)));
}

void ImeAdapterAndroid::SetEditableSelectionOffsets(
    JNIEnv*,
    const JavaParamRef<jobject>&,
    int start,
    int end) {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;

  input_handler->SetEditableSelectionOffsets(start, end);
}

void ImeAdapterAndroid::SetCharacterBounds(
    const std::vector<gfx::RectF>& character_bounds) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = java_ime_adapter_.get(env);
  if (obj.is_null())
    return;

  const size_t coordinates_array_size = character_bounds.size() * 4;
  std::unique_ptr<float[]> coordinates_array(new float[coordinates_array_size]);
  for (size_t i = 0; i < character_bounds.size(); ++i) {
    const gfx::RectF& rect = character_bounds[i];
    const size_t coordinates_array_index = i * 4;
    coordinates_array[coordinates_array_index + 0] = rect.x();
    coordinates_array[coordinates_array_index + 1] = rect.y();
    coordinates_array[coordinates_array_index + 2] = rect.right();
    coordinates_array[coordinates_array_index + 3] = rect.bottom();
  }
  Java_ImeAdapterImpl_setCharacterBounds(
      env, obj,
      base::android::ToJavaFloatArray(env, coordinates_array.get(),
                                      coordinates_array_size));
}

void ImeAdapterAndroid::SetComposingRegion(JNIEnv*,
                                           const JavaParamRef<jobject>&,
                                           int start,
                                           int end) {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;

  std::vector<ui::ImeTextSpan> ime_text_spans;
  ime_text_spans.push_back(
      ui::ImeTextSpan(ui::ImeTextSpan::Type::kComposition, 0, end - start,
                      ui::ImeTextSpan::Thickness::kThin, SK_ColorTRANSPARENT,
                      SK_ColorTRANSPARENT, std::vector<std::string>()));

  input_handler->SetCompositionFromExistingText(start, end, ime_text_spans);
}

void ImeAdapterAndroid::DeleteSurroundingText(JNIEnv*,
                                              const JavaParamRef<jobject>&,
                                              int before,
                                              int after) {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->DeleteSurroundingText(before, after);
}

void ImeAdapterAndroid::DeleteSurroundingTextInCodePoints(
    JNIEnv*,
    const JavaParamRef<jobject>&,
    int before,
    int after) {
  auto* input_handler = GetFocusedFrameInputHandler();
  if (!input_handler)
    return;
  input_handler->DeleteSurroundingTextInCodePoints(before, after);
}

bool ImeAdapterAndroid::RequestTextInputStateUpdate(
    JNIEnv* env,
    const JavaParamRef<jobject>&) {
  RenderWidgetHostImpl* rwhi = GetFocusedWidget();
  if (!rwhi)
    return false;
  rwhi->GetWidgetInputHandler()->RequestTextInputStateUpdate();
  return true;
}

void ImeAdapterAndroid::RequestCursorUpdate(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    bool immediate_request,
    bool monitor_request) {
  RenderWidgetHostImpl* rwhi = GetFocusedWidget();
  if (!rwhi)
    return;
  rwhi->GetWidgetInputHandler()->RequestCompositionUpdates(immediate_request,
                                                           monitor_request);
}

RenderWidgetHostImpl* ImeAdapterAndroid::GetFocusedWidget() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return rwhva_ ? rwhva_->GetFocusedWidget() : nullptr;
}

RenderFrameHost* ImeAdapterAndroid::GetFocusedFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // We get the focused frame from the WebContents of the page. Although
  // |rwhva_->GetFocusedWidget()| does a similar thing, there is no direct way
  // to get a RenderFrameHost from its RWH.
  if (!rwhva_)
    return nullptr;
  RenderWidgetHostImpl* rwh =
      RenderWidgetHostImpl::From(rwhva_->GetRenderWidgetHost());
  if (!rwh || !rwh->delegate())
    return nullptr;

  if (auto* contents = rwh->delegate()->GetAsWebContents())
    return contents->GetFocusedFrame();

  return nullptr;
}

mojom::FrameInputHandler* ImeAdapterAndroid::GetFocusedFrameInputHandler() {
  auto* focused_frame = static_cast<RenderFrameHostImpl*>(GetFocusedFrame());
  if (!focused_frame)
    return nullptr;
  return focused_frame->GetFrameInputHandler();
}

std::vector<ui::ImeTextSpan> ImeAdapterAndroid::GetImeTextSpansFromJava(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& obj,
    const base::android::JavaParamRef<jobject>& text,
    const base::string16& text16) {
  std::vector<ui::ImeTextSpan> ime_text_spans;
  // Iterate over spans in |text|, dispatch those that we care about (e.g.,
  // BackgroundColorSpan) to a matching callback (e.g.,
  // AppendBackgroundColorSpan()), and populate |ime_text_spans|.
  Java_ImeAdapterImpl_populateImeTextSpansFromJava(
      env, obj, text, reinterpret_cast<jlong>(&ime_text_spans));

  std::sort(ime_text_spans.begin(), ime_text_spans.end(),
            [](const ui::ImeTextSpan& span1, const ui::ImeTextSpan& span2) {
              return span1.start_offset < span2.start_offset;
            });

  return ime_text_spans;
}

}  // namespace content
