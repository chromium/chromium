// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/file_path_conversion.h"

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

base::FilePath StringToFilePath(const String& str) {
  if (str.IsEmpty())
    return base::FilePath();

  if (!str.Is8Bit()) {
    return base::FilePath::FromUTF16Unsafe(
        base::StringPiece16(str.Characters16(), str.length()));
  }

#if defined(OS_POSIX)
  StringUTF8Adaptor utf8(str);
  return base::FilePath::FromUTF8Unsafe(utf8.AsStringPiece());
#else
  const LChar* data8 = str.Characters8();
  return base::FilePath::FromUTF16Unsafe(
      base::string16(data8, data8 + str.length()));
#endif
}

base::FilePath WebStringToFilePath(const WebString& web_string) {
  return StringToFilePath(web_string);
}

WebString FilePathToWebString(const base::FilePath& path) {
  if (path.empty())
    return WebString();

#if defined(OS_POSIX)
  return WebString::FromUTF8(path.value());
#else
  return WebString::FromUTF16(path.AsUTF16Unsafe());
#endif
}

String FilePathToString(const base::FilePath& path) {
  return FilePathToWebString(path);
}

}  // namespace blink
