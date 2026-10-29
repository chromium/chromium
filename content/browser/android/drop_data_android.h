// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_DROP_DATA_ANDROID_H_
#define CONTENT_BROWSER_ANDROID_DROP_DATA_ANDROID_H_

#include <jni.h>

#include <optional>

#include "base/android/jni_android.h"
#include "base/unguessable_token.h"
#include "base/values.h"
#include "content/common/content_export.h"
#include "content/public/common/drop_data.h"

namespace ui {
class DragEventAndroid;
}

namespace content {

class WebContentsViewDragSecurityInfo;

// Generate a java equivalent DropData object from |drop_data|. Note that the
// timeline of these object are not equivalent.
base::android::ScopedJavaLocalRef<jobject> ToJavaDropData(
    const DropData& drop_data);

// Populates |drop_data| with data from the Android |event|.
// Uses `drag_security_info` to determine if the drag contains restricted
// file data (e.g., cross-origin images). If restricted, prevents populating
// `filenames` and any future file-related fields to avoid leaking data to
// the renderer.
CONTENT_EXPORT void PopulateDropDataFromEvent(
    const ui::DragEventAndroid& event,
    const WebContentsViewDragSecurityInfo& drag_security_info,
    DropData* drop_data);

// Parses and returns the custom data JSON payload from the Android |event|.
// Returns std::nullopt if the event does not contain custom data or parsing
// fails. If successful, the returned base::Value is guaranteed to be a
// dictionary.
CONTENT_EXPORT std::optional<base::Value> ParseCustomDataFromEvent(
    const ui::DragEventAndroid& event);

// Parses and returns the drag ID (`DropData::drag_id`) that a drag started from
// web content carries in its ClipDescription extras. Any app can set this
// extra, so the result is unverified: it must not be treated as proof that the
// drag came from this browser. Returns std::nullopt if the event has no drag
// ID or it fails to parse.
CONTENT_EXPORT std::optional<base::UnguessableToken> ParseDragIdFromEvent(
    const ui::DragEventAndroid& event);

}  // namespace content
#endif  // CONTENT_BROWSER_ANDROID_DROP_DATA_ANDROID_H_
