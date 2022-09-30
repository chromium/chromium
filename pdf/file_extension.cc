// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/file_extension.h"

#include <array>
#include <iterator>
#include <string>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"

#if BUILDFLAG(IS_WIN)
#include "base/strings/utf_string_conversions.h"
#endif

namespace chrome_pdf {

// The order of the entries should always match `ViewFileType` in
// tools/metrics/histograms/enums.xml and the indexes defined in
// `ExtensionIndex`.
constexpr std::array<const char*, 76> kFileExtensions = {
    "other",     ".3ga",         ".3gp",
    ".aac",      ".alac",        ".asf",
    ".avi",      ".bmp",         ".csv",
    ".doc",      ".docx",        ".flac",
    ".gif",      ".jpeg",        ".jpg",
    ".log",      ".m3u",         ".m3u8",
    ".m4a",      ".m4v",         ".mid",
    ".mkv",      ".mov",         ".mp3",
    ".mp4",      ".mpg",         ".odf",
    ".odp",      ".ods",         ".odt",
    ".oga",      ".ogg",         ".ogv",
    ".pdf",      ".png",         ".ppt",
    ".pptx",     ".ra",          ".ram",
    ".rar",      ".rm",          ".rtf",
    ".wav",      ".webm",        ".webp",
    ".wma",      ".wmv",         ".xls",
    ".xlsx",     ".crdownload",  ".crx",
    ".dmg",      ".exe",         ".html",
    ".htm",      ".jar",         ".ps",
    ".torrent",  ".txt",         ".zip",
    "directory", "no extension", "unknown extension",
    ".mhtml",    ".gdoc",        ".gsheet",
    ".gslides",  ".arw",         ".cr2",
    ".dng",      ".nef",         ".nrw",
    ".orf",      ".raf",         ".rw2",
    ".tini",
};

static_assert(kFileExtensions.size() ==
              static_cast<size_t>(ExtensionIndex::kMaxValue) + 1);

enum ExtensionIndex FileNameToExtensionIndex(const std::u16string& file_name) {
  const base::FilePath::StringType extension_str =
      base::FilePath::FromUTF16Unsafe(file_name).Extension();
  if (extension_str.empty())
    return ExtensionIndex::kEmptyExt;

  // All known extensions are ASCII characters. So when an extension contains
  // non-ASCII characters, this extension is not recognizable.
  if (!base::IsStringASCII(extension_str))
    return ExtensionIndex::kOtherExt;

  const base::FilePath::StringType extension_str_lower =
      base::ToLowerASCII(extension_str);

#if BUILDFLAG(IS_WIN)
  const std::string extension = base::WideToUTF8(extension_str_lower);
#else
  const std::string& extension = extension_str_lower;
#endif

  auto* const* it = base::ranges::find(kFileExtensions, extension);
  if (it == kFileExtensions.end())
    return ExtensionIndex::kOtherExt;

  const int distance = std::distance(kFileExtensions.begin(), it);
  DCHECK_GT(distance, 0);
  DCHECK_LT(static_cast<size_t>(distance), kFileExtensions.size());
  return static_cast<enum ExtensionIndex>(distance);
}

}  // namespace chrome_pdf
