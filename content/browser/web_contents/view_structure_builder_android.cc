// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_

#include "content/browser/web_contents/view_structure_builder_android.h"
#include "content/public/android/content_jni_headers/ViewStructureBuilder_jni.h"

namespace content {

void ViewStructureBuilder_populateViewStructureNode(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    const base::android::JavaRef<jstring>& text,
    jboolean has_selection,
    JniIntWrapper sel_start,
    JniIntWrapper sel_end,
    JniIntWrapper color,
    JniIntWrapper bgcolor,
    jfloat size,
    jboolean bold,
    jboolean italic,
    jboolean underline,
    jboolean line_through,
    const base::android::JavaRef<jstring>& class_name,
    JniIntWrapper child_count) {
  Java_ViewStructureBuilder_populateViewStructureNode(
      env, obj, node, text, has_selection, sel_start, sel_end, color, bgcolor,
      size, bold, italic, underline, line_through, class_name, child_count);
}

void ViewStructureBuilder_setViewStructureNodeBounds(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    jboolean is_root_node,
    JniIntWrapper parent_relative_left,
    JniIntWrapper parent_relative_top,
    JniIntWrapper width,
    JniIntWrapper height) {
  Java_ViewStructureBuilder_setViewStructureNodeBounds(
      env, obj, node, is_root_node, parent_relative_left, parent_relative_top,
      width, height);
}

void ViewStructureBuilder_setViewStructureNodeHtmlInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    const base::android::JavaRef<jstring>& html_tag,
    const base::android::JavaRef<jstring>& css_display,
    const base::android::JavaRef<jobjectArray>& html_attributes) {
  Java_ViewStructureBuilder_setViewStructureNodeHtmlInfo(
      env, obj, node, html_tag, css_display, html_attributes);
}

void ViewStructureBuilder_setViewStructureNodeHtmlMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    const base::android::JavaRef<jobjectArray>& metadata_strings) {
  Java_ViewStructureBuilder_setViewStructureNodeHtmlMetadata(env, obj, node,
                                                             metadata_strings);
}

void ViewStructureBuilder_commitViewStructureNode(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node) {
  Java_ViewStructureBuilder_commitViewStructureNode(env, obj, node);
}

base::android::ScopedJavaLocalRef<jobject>
ViewStructureBuilder_addViewStructureNodeChild(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    JniIntWrapper index) {
  return Java_ViewStructureBuilder_addViewStructureNodeChild(env, obj, node,
                                                             index);
}

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_
