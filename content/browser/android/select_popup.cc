// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/select_popup.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/heap_array.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/browser/web_contents/web_contents_view_android.h"
#include "ui/gfx/geometry/rect_f.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "content/public/android/content_jni_headers/SelectPopup_jni.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace content {

namespace {

// Describes the type and enabled state of a select popup item.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.input
enum PopupItemType {
  // Popup item is of type group
  POPUP_ITEM_TYPE_GROUP,

  // Popup item is disabled
  POPUP_ITEM_TYPE_DISABLED,

  // Popup item is enabled
  POPUP_ITEM_TYPE_ENABLED,
};

}  // namespace

SelectPopup::SelectPopup(WebContentsImpl* web_contents)
    : web_contents_(web_contents) {
  JNIEnv* env = AttachCurrentThread();
  java_obj_ = JavaObjectWeakGlobalRef(
      env, Java_SelectPopup_create(env, web_contents_->GetJavaWebContents(),
                                   reinterpret_cast<intptr_t>(this)));
}

SelectPopup::~SelectPopup() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_obj_.get(env);
  if (j_obj.is_null())
    return;
  Java_SelectPopup_onNativeDestroyed(env, j_obj);
  popup_client_.reset();
}

void SelectPopup::ShowMenu(
    mojo::PendingRemote<blink::mojom::PopupMenuClient> popup_client,
    const gfx::Rect& bounds,
    std::vector<blink::mojom::MenuItemPtr> items,
    int selected_item,
    bool multiple,
    bool right_aligned) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_obj_.get(env);
  if (j_obj.is_null())
    return;

  // Hide the popup menu if the mojo connection is still open.
  if (popup_client_)
    HideMenu();

  // For multi-select list popups we find the list of previous selections by
  // iterating through the items. But for single selection popups we take the
  // given |selected_item| as is.
  ScopedJavaLocalRef<jintArray> selected_array;
  if (multiple) {
    auto native_selected_array = base::HeapArray<jint>::WithSize(items.size());
    size_t selected_count = 0;
    for (size_t i = 0; i < items.size(); ++i) {
      if (items[i]->checked)
        native_selected_array[selected_count++] = i;
    }

    selected_array =
        ScopedJavaLocalRef<jintArray>(env, env->NewIntArray(selected_count));
    env->SetIntArrayRegion(selected_array.obj(), 0, selected_count,
                           native_selected_array.data());
  } else {
    selected_array = ScopedJavaLocalRef<jintArray>(env, env->NewIntArray(1));
    jint value = selected_item;
    env->SetIntArrayRegion(selected_array.obj(), 0, 1, &value);
  }

  ScopedJavaLocalRef<jintArray> enabled_array(env,
                                              env->NewIntArray(items.size()));
  std::vector<std::string> labels;
  labels.reserve(items.size());
  for (size_t i = 0; i < items.size(); ++i) {
    labels.push_back(items[i]->label.value_or(""));
    jint enabled = (items[i]->type == blink::mojom::MenuItem::Type::kGroup
                        ? POPUP_ITEM_TYPE_GROUP
                        : (items[i]->enabled ? POPUP_ITEM_TYPE_ENABLED
                                             : POPUP_ITEM_TYPE_DISABLED));
    env->SetIntArrayRegion(enabled_array.obj(), i, 1, &enabled);
  }
  ScopedJavaLocalRef<jobjectArray> items_array(
      base::android::ToJavaArrayOfStrings(env, labels));
  ui::ViewAndroid* view = web_contents_->GetNativeView();
  popup_view_ = view->AcquireAnchorView();
  const ScopedJavaLocalRef<jobject> popup_view = popup_view_.view();
  if (popup_view.is_null())
    return;

  popup_client_.Bind(std::move(popup_client));
  popup_client_.set_disconnect_handler(
      base::BindOnce(&SelectPopup::HideMenu, base::Unretained(this)));

  // |bounds| is in physical pixels.
  gfx::RectF bounds_dip = gfx::RectF(bounds);
  bounds_dip.Scale(1 / web_contents_->GetNativeView()->GetDipScale());
  view->SetAnchorRect(popup_view, bounds_dip);
  Java_SelectPopup_show(
      env, j_obj, popup_view, reinterpret_cast<jlong>(popup_client_.get()),
      items_array, enabled_array, multiple, selected_array, right_aligned);
}

void SelectPopup::HideMenu() {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> j_obj = java_obj_.get(env);
  if (!j_obj.is_null())
    Java_SelectPopup_hideWithoutCancel(env, j_obj);
  popup_view_.Reset();
  popup_client_.reset();
}

void SelectPopup::SelectMenuItems(JNIEnv* env,
                                  const JavaParamRef<jobject>& obj,
                                  jlong selectPopupDelegate,
                                  const JavaParamRef<jintArray>& indices) {
  blink::mojom::PopupMenuClient* popup_client_raw_ptr =
      reinterpret_cast<blink::mojom::PopupMenuClient*>(selectPopupDelegate);
  DCHECK(popup_client_raw_ptr && popup_client_.get() == popup_client_raw_ptr);

  if (indices == NULL) {
    popup_client_->DidCancel();
    return;
  }

  std::vector<int> selected_indices;
  base::android::JavaIntArrayToIntVector(env, indices, &selected_indices);
  popup_client_->DidAcceptIndices(selected_indices);
}

}  // namespace content
