// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "third_party/blink/public/platform/file_path_conversion.h"

#include <string_view>

#include "base/files/file_path.h"
#include "build/build_config.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_utf8_adaptor.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

base::FilePath StringToFilePath(const String& str) {
  if (str.empty())
    return base::FilePath();

  if (!str.Is8Bit()) {
    return base::FilePath::FromUTF16Unsafe(
        std::u16string_view(str.Characters16(), str.length()));
  }

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  StringUTF8Adaptor utf8(str);
  return base::FilePath::FromUTF8Unsafe(utf8.AsStringView());
#else
  const LChar* data8 = str.Characters8();
  return base::FilePath::FromUTF16Unsafe(
      std::u16string(data8, data8 + str.length()));
#endif
}

base::FilePath WebStringToFilePath(const WebString& web_string) {
  return StringToFilePath(web_string);
}

WebString FilePathToWebString(const base::FilePath& path) {
  if (path.empty())
    return WebString();

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return WebString::FromUTF8(path.value());
#else
  return WebString::FromUTF16(path.AsUTF16Unsafe());
#endif
}

String FilePathToString(const base::FilePath& path) {
  return FilePathToWebString(path);
}

}  // namespace blink
