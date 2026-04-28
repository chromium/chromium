// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/drop_data_android.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "ui/events/android/drag_event_android.h"
#include "url/android/gurl_android.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "ui/android/ui_android_jni_headers/DropDataAndroid_jni.h"

using base::android::AttachCurrentThread;
using base::android::ConvertUTF16ToJavaString;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using base::android::ToJavaByteArray;

namespace content {

// This function refers to //content/browser/renderer_host/data_transfer_util.cc
// on how data fields are populated. If data_transfer_util.cc ever changed, this
// function will need updates to keep in sync.
ScopedJavaLocalRef<jobject> ToJavaDropData(const DropData& drop_data) {
  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jstring> jtext;
  if (drop_data.text) {
    jtext = ConvertUTF16ToJavaString(env, *drop_data.text);
  }

  ScopedJavaLocalRef<jobject> jgurl;
  if (!drop_data.url_infos.empty()) {
    const ui::ClipboardUrlInfo& url_info = drop_data.url_infos.front();
    jgurl = url::GURLAndroid::FromNativeGURL(env, url_info.url);
    jtext = ConvertUTF16ToJavaString(env, url_info.title);
  }

  // If file_contents is not empty, user is trying to drag image out of the
  // web contents. If the image contains a link, the link URL, represented by
  // |jgurl|, will be ignored.
  // drop_data.file_contents_source_url represents the image source URL.
  ScopedJavaLocalRef<jbyteArray> jimage_bytes;
  ScopedJavaLocalRef<jstring> jimage_extension;
  ScopedJavaLocalRef<jstring> jimage_filename;
  if (!drop_data.file_contents.empty()) {
    jimage_bytes = ToJavaByteArray(env, drop_data.file_contents);
    jimage_extension = ConvertUTF8ToJavaString(
        env, drop_data.file_contents_filename_extension);
    std::optional<base::FilePath> filename =
        drop_data.GetSafeFilenameForImageFileContents();
    if (filename) {
      jimage_filename =
          ConvertUTF16ToJavaString(env, filename->LossyDisplayName());
    } else {
      // Use the current timestamp as the image file name in case the file name
      // is not retrieved from the source.
      jimage_filename = ConvertUTF8ToJavaString(
          env, base::NumberToString(
                   base::Time::Now().since_origin().InMilliseconds()) +
                   "." + drop_data.file_contents_filename_extension);
    }
  }

  base::DictValue custom_data_dict;
  for (const auto& pair : drop_data.custom_data) {
    // Key must be converted to UTF-8 as base::Value only supports UTF-8 keys.
    // Value can remain UTF-16 as there is a specific overload for it.
    custom_data_dict.Set(base::UTF16ToUTF8(pair.first), pair.second);
  }
  std::optional<std::string> custom_data_json =
      base::WriteJson(base::Value(std::move(custom_data_dict)));
  ScopedJavaLocalRef<jstring> jcustom_data;
  if (custom_data_json) {
    jcustom_data = ConvertUTF8ToJavaString(env, *custom_data_json);
  }

  ScopedJavaLocalRef<jstring> jeffect_allowed;
  if (drop_data.source_effect_allowed) {
    jeffect_allowed =
        ConvertUTF16ToJavaString(env, *drop_data.source_effect_allowed);
  }

  return ui::Java_DropDataAndroid_create(env, jtext, jgurl, jimage_bytes,
                                         jimage_extension, jimage_filename,
                                         jcustom_data, jeffect_allowed);
}  // ToJavaDropData

namespace {
void PopulateCustomDataFromEvent(JNIEnv* env,
                                 const ui::DragEventAndroid& event,
                                 DropData* drop_data) {
  if (event.GetJavaCustomData().is_null()) {
    return;
  }
  std::string custom_data_json =
      base::android::ConvertJavaStringToUTF8(env, event.GetJavaCustomData());
  if (custom_data_json.empty()) {
    return;
  }
  std::optional<base::Value> value =
      base::JSONReader::Read(custom_data_json, 0);
  if (!value || !value->is_dict()) {
    return;
  }
  const base::DictValue* dict = value->GetIfDict();
  for (auto item : *dict) {
    const std::string* str = item.second.GetIfString();
    if (!str) {
      continue;
    }
    drop_data->custom_data.emplace(base::UTF8ToUTF16(item.first),
                                   base::UTF8ToUTF16(*str));
  }
}
}  // namespace

void PopulateDropDataFromEvent(const ui::DragEventAndroid& event,
                               DropData* drop_data) {
  JNIEnv* env = AttachCurrentThread();

  std::vector<std::vector<std::string>> filenames;
  if (!event.GetJavaFilenames().is_null()) {
    base::android::Java2dStringArrayTo2dStringVector(
        env, event.GetJavaFilenames(), &filenames);
  }
  for (const auto& info : filenames) {
    CHECK_EQ(info.size(), 2u);
    drop_data->filenames.emplace_back(base::FilePath(info[0]),
                                      base::FilePath(info[1]));
  }

  if (!event.GetJavaText().is_null()) {
    drop_data->text =
        base::android::ConvertJavaStringToUTF16(env, event.GetJavaText());
  }
  if (!event.GetJavaHtml().is_null()) {
    drop_data->html =
        base::android::ConvertJavaStringToUTF16(env, event.GetJavaHtml());
  }
  if (!event.GetJavaUrl().is_null()) {
    GURL url(base::android::ConvertJavaStringToUTF16(env, event.GetJavaUrl()));
    drop_data->url_infos.emplace_back(std::move(url), std::u16string());
  }

  // Handle custom data.
  PopulateCustomDataFromEvent(env, event, drop_data);

  // Handle effectAllowed.
  if (!event.GetJavaEffectAllowed().is_null()) {
    drop_data->source_effect_allowed = base::android::ConvertJavaStringToUTF16(
        env, event.GetJavaEffectAllowed());
  }
}

}  // namespace content

DEFINE_JNI(DropDataAndroid)
