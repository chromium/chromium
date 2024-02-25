// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util.h"

#include <set>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/filename_util_internal.h"
#include "net/base/net_string_util.h"
#include "net/base/url_util.h"
#include "net/http/http_content_disposition.h"
#include "url/gurl.h"

namespace net {

// Prefix to prepend to get a file URL.
static const char kFileURLPrefix[] = "file:///";

GURL FilePathToFileURL(const base::FilePath& path) {
  // Produce a URL like "file:///C:/foo" for a regular file, or
  // "file://///server/path" for UNC. The URL canonicalizer will fix up the
  // latter case to be the canonical UNC form: "file://server/path"
  std::string url_string(kFileURLPrefix);

  // GURL() strips some whitespace and trailing control chars which are valid
  // in file paths. It also interprets chars such as `%;#?` and maybe `\`, so we
  // must percent encode these first. Reserve max possible length up front.
  std::string utf8_path = path.AsUTF8Unsafe();
  url_string.reserve(url_string.size() + (3 * utf8_path.size()));

  for (auto c : utf8_path) {
    if (c == '%' || c == ';' || c == '#' || c == '?' ||
#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
        c == '\\' ||
#endif
        c <= ' ') {
      url_string += '%';
      base::AppendHexEncodedByte(static_cast<uint8_t>(c), url_string);
    } else {
      url_string += c;
    }
  }

  return GURL(url_string);
}

bool FileURLToFilePath(const GURL& url, base::FilePath* file_path) {
  *file_path = base::FilePath();
  base::FilePath::StringType& file_path_str =
      const_cast<base::FilePath::StringType&>(file_path->value());
  file_path_str.clear();

  if (!url.is_valid())
    return false;

  // We may want to change this to a CHECK in the future.
  if (!url.SchemeIsFile())
    return false;

#if BUILDFLAG(IS_WIN)
  std::string path;
  std::string host = url.host();
  if (host.empty()) {
    // URL contains no host, the path is the filename. In this case, the path
    // will probably be preceded with a slash, as in "/C:/foo.txt", so we
    // trim out that here.
    path = url.path();
    size_t first_non_slash = path.find_first_not_of("/\\");
    if (first_non_slash != std::string::npos && first_non_slash > 0)
      path.erase(0, first_non_slash);
  } else {
    // URL contains a host: this means it's UNC. We keep the preceding slash
    // on the path.
    path = "\\\\";
    path.append(host);
    path.append(url.path());
  }
  std::replace(path.begin(), path.end(), '/', '\\');
#else   // BUILDFLAG(IS_WIN)
  // On POSIX, there's no obvious interpretation of file:// URLs with a host.
  // Usually, remote mounts are still mounted onto the local filesystem.
  // Therefore, we discard all URLs that are not obviously local to prevent
  // spoofing attacks using file:// URLs. See crbug.com/881675.
  if (!url.host().empty() && !net::IsLocalhost(url)) {
    return false;
  }
  std::string path = url.path();
#endif  // !BUILDFLAG(IS_WIN)

  if (path.empty())
    return false;

  // "%2F" ('/') results in failure, because it represents a literal '/'
  // character in a path segment (not a path separator). If this were decoded,
  // it would be interpreted as a path separator on both POSIX and Windows (note
  // that Firefox *does* decode this, but it was decided on
  // https://crbug.com/585422 that this represents a potential security risk).
  // It isn't correct to keep it as "%2F", so this just fails. This is fine,
  // because '/' is not a valid filename character on either POSIX or Windows.
  //
  // A valid URL may include "%00" (NULL) in its path (see
  // https://crbug.com/1400251), which is considered an illegal filename and
  // results in failure.
  std::set<unsigned char> illegal_encoded_bytes{'/', '\0'};

#if BUILDFLAG(IS_WIN)
  // "%5C" ('\\') on Windows results in failure, for the same reason as '/'
  // above. On POSIX, "%5C" simply decodes as '\\', a valid filename character.
  illegal_encoded_bytes.insert('\\');
#endif

  if (base::ContainsEncodedBytes(path, illegal_encoded_bytes))
    return false;

  // Unescape all percent-encoded sequences, including blocked-for-display
  // characters, control characters and invalid UTF-8 byte sequences.
  // Percent-encoded bytes are not meaningful in a file system.
  path = base::UnescapeBinaryURLComponent(path);

#if BUILDFLAG(IS_WIN)
  if (base::IsStringUTF8(path)) {
    file_path_str.assign(base::UTF8ToWide(path));
    // We used to try too hard and see if |path| made up entirely of
    // the 1st 256 characters in the Unicode was a zero-extended UTF-16.
    // If so, we converted it to 'Latin-1' and checked if the result was UTF-8.
    // If the check passed, we converted the result to UTF-8.
    // Otherwise, we treated the result as the native OS encoding.
    // However, that led to http://crbug.com/4619 and http://crbug.com/14153
  } else {
    // Not UTF-8, assume encoding is native codepage and we're done. We know we
    // are giving the conversion function a nonempty string, and it may fail if
    // the given string is not in the current encoding and give us an empty
    // string back. We detect this and report failure.
    file_path_str = base::SysNativeMBToWide(path);
  }
#else   // BUILDFLAG(IS_WIN)
  // Collapse multiple path slashes into a single path slash.
  std::string new_path;
  do {
    new_path = path;
    base::ReplaceSubstringsAfterOffset(&new_path, 0, "//", "/");
    path.swap(new_path);
  } while (new_path != path);

  file_path_str.assign(path);
#endif  // !BUILDFLAG(IS_WIN)

  return !file_path_str.empty();
}

void GenerateSafeFileName(const std::string& mime_type,
                          bool ignore_extension,
                          base::FilePath* file_path) {
  // Make sure we get the right file extension
  EnsureSafeExtension(mime_type, ignore_extension, file_path);

#if BUILDFLAG(IS_WIN)
  // Prepend "_" to the file name if it's a reserved name
  base::FilePath::StringType leaf_name = file_path->BaseName().value();
  DCHECK(!leaf_name.empty());
  if (IsReservedNameOnWindows(leaf_name)) {
    leaf_name = base::FilePath::StringType(FILE_PATH_LITERAL("_")) + leaf_name;
    *file_path = file_path->DirName();
    if (file_path->value() == base::FilePath::kCurrentDirectory) {
      *file_path = base::FilePath(leaf_name);
    } else {
      *file_path = file_path->Append(leaf_name);
    }
  }
#endif
}

bool IsReservedNameOnWindows(const base::FilePath::StringType& filename) {
  // This list is taken from the MSDN article "Naming a file"
  // http://msdn2.microsoft.com/en-us/library/aa365247(VS.85).aspx
  // I also added clock$ because GetSaveFileName seems to consider it as a
  // reserved name too.
  static const char* const known_devices[] = {
      "con",  "prn",  "aux",  "nul",  "com1", "com2", "com3",  "com4",
      "com5", "com6", "com7", "com8", "com9", "lpt1", "lpt2",  "lpt3",
      "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9", "clock$"};
#if BUILDFLAG(IS_WIN)
  std::string filename_lower = base::ToLowerASCII(base::WideToUTF8(filename));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::string filename_lower = base::ToLowerASCII(filename);
#endif

  for (const char* const device : known_devices) {
    // Check for an exact match, or a "DEVICE." prefix.
    size_t len = strlen(device);
    if (filename_lower.starts_with(device) &&
        (filename_lower.size() == len || filename_lower[len] == '.')) {
      return true;
    }
  }

  static const char* const magic_names[] = {
      // These file names are used by the "Customize folder" feature of the
      // shell.
      "desktop.ini",
      "thumbs.db",
  };

  for (const char* const magic_name : magic_names) {
    if (filename_lower == magic_name)
      return true;
  }

  return false;
}

}  // namespace net
