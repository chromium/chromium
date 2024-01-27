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
    const jni_zero::JavaRef<jobject>& obj,
    const jni_zero::JavaRef<jobject>& node,
    const jni_zero::JavaRef<jstring>& text,
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
    const jni_zero::JavaRef<jstring>& class_name,
    JniIntWrapper child_count) {
  Java_ViewStructureBuilder_populateViewStructureNode(
      env, obj, node, text, has_selection, sel_start, sel_end, color, bgcolor,
      size, bold, italic, underline, line_through, class_name, child_count);
}

void ViewStructureBuilder_setViewStructureNodeBounds(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const jni_zero::JavaRef<jobject>& node,
    jboolean is_root_node,
    JniIntWrapper parent_relative_left,
    JniIntWrapper parent_relative_top,
    JniIntWrapper width,
    JniIntWrapper height,
    JniIntWrapper unclipped_left,
    JniIntWrapper unclipped_top,
    JniIntWrapper unclipped_width,
    JniIntWrapper unclipped_height,
    JniIntWrapper page_absolute_left,
    JniIntWrapper page_absolute_top,
    JniIntWrapper page_absolute_width,
    JniIntWrapper page_absolute_height) {
  Java_ViewStructureBuilder_setViewStructureNodeBounds(
      env, obj, node, is_root_node, parent_relative_left, parent_relative_top,
      width, height, unclipped_left, unclipped_top, unclipped_width,
      unclipped_height, page_absolute_left, page_absolute_top,
      page_absolute_width, page_absolute_height);
}

void ViewStructureBuilder_setViewStructureNodeHtmlInfo(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const jni_zero::JavaRef<jobject>& node,
    const jni_zero::JavaRef<jstring>& html_tag,
    const jni_zero::JavaRef<jstring>& css_display,
    const jni_zero::JavaRef<jobjectArray>& html_attributes) {
  Java_ViewStructureBuilder_setViewStructureNodeHtmlInfo(
      env, obj, node, html_tag, css_display, html_attributes);
}

void ViewStructureBuilder_setViewStructureNodeHtmlMetadata(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const jni_zero::JavaRef<jobject>& node,
    const jni_zero::JavaRef<jobjectArray>& metadata_strings) {
  Java_ViewStructureBuilder_setViewStructureNodeHtmlMetadata(env, obj, node,
                                                             metadata_strings);
}

void ViewStructureBuilder_commitViewStructureNode(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const jni_zero::JavaRef<jobject>& node) {
  Java_ViewStructureBuilder_commitViewStructureNode(env, obj, node);
}

jni_zero::ScopedJavaLocalRef<jobject>
ViewStructureBuilder_addViewStructureNodeChild(
    JNIEnv* env,
    const jni_zero::JavaRef<jobject>& obj,
    const jni_zero::JavaRef<jobject>& node,
    JniIntWrapper index) {
  return Java_ViewStructureBuilder_addViewStructureNodeChild(env, obj, node,
                                                             index);
}

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_
