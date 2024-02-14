// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_
#define CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "third_party/jni_zero/jni_zero.h"

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
    JniIntWrapper child_count);

void ViewStructureBuilder_setViewStructureNodeBounds(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
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
    JniIntWrapper page_absolute_height);

void ViewStructureBuilder_setViewStructureNodeHtmlInfo(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    const base::android::JavaRef<jstring>& html_tag,
    const base::android::JavaRef<jstring>& css_display,
    const base::android::JavaRef<jobjectArray>& html_attributes);

void ViewStructureBuilder_setViewStructureNodeHtmlMetadata(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    const base::android::JavaRef<jobjectArray>& metadata_strings);

void ViewStructureBuilder_commitViewStructureNode(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node);

base::android::ScopedJavaLocalRef<jobject>
ViewStructureBuilder_addViewStructureNodeChild(
    JNIEnv* env,
    const base::android::JavaRef<jobject>& obj,
    const base::android::JavaRef<jobject>& node,
    JniIntWrapper index);

}  // namespace content

#endif  // CONTENT_BROWSER_WEB_CONTENTS_VIEW_STRUCTURE_BUILDER_ANDROID_H_
