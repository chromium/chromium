// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/android/drop_data_android_helper.h"

#include "base/android/jni_android.h"
#include "base/files/file_path.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/events/android/drop_data_android.h"
#include "url/gurl.h"

using base::UTF8ToUTF16;

namespace content {

// This function refers to //content/browser/renderer_host/data_transfer_util.cc
// on how data fields are populated. If data_transfer_util.cc ever changed, this
// function will need updates to keep in sync.
base::android::ScopedJavaLocalRef<jobject> ToJavaDropData(
    const DropData& drop_data) {
  std::u16string text;
  if (drop_data.text) {
    text = *drop_data.text;
  }

  GURL gurl = drop_data.url;
  if (!gurl.is_empty()) {
    text = drop_data.url_title;
  }

  // If file_contents is not empty, user is trying to drag image out of the
  // web contents. If the image contains a link, the link URL, represented by
  // |jgurl|, will be added to the image clip data.
  // drop_data.file_contents_source_url represents the image source URL;
  // drop_data.url represents the URL that is linked to the image, which may not
  // necessarily be the image source URL and is the desired URL to be added to
  // the image clip data.
  std::string file_content;
  std::string image_extension;
  std::u16string image_filename;
  if (!drop_data.file_contents.empty()) {
    file_content = drop_data.file_contents;
    image_extension = drop_data.file_contents_filename_extension;
    absl::optional<base::FilePath> filename =
        drop_data.GetSafeFilenameForImageFileContents();
    if (filename) {
      image_filename = filename->LossyDisplayName();
    } else {
      // Use the current timestamp as the image file name in case the file name
      // is not retrieved from the source.
      image_filename =
          UTF8ToUTF16(base::NumberToString(
                          base::Time::Now().since_origin().InMilliseconds()) +
                      "." + image_extension);
    }
  }

  ui::DropDataAndroid drop_data_android = ui::DropDataAndroid::Create(
      text, gurl, file_content, image_extension, image_filename);

  return drop_data_android.GetJavaObject();

}  // ToJavaDropData

}  // namespace content
