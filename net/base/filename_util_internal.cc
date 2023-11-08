// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/filename_util_internal.h"

#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/strings/escape.h"
#include "base/strings/string_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "net/base/net_string_util.h"
#include "net/http/http_content_disposition.h"
#include "url/gurl.h"

namespace net {

namespace {

// Examines the current extension in |file_name| and tries to return the correct
// extension the file should actually be using.  Used by EnsureSafeExtension.
// All other code should use EnsureSafeExtension, as it includes additional
// safety checks.
base::FilePath::StringType GetCorrectedExtensionUnsafe(
    const std::string& mime_type,
    bool ignore_extension,
    const base::FilePath& file_name) {
  // See if the file name already contains an extension.
  base::FilePath::StringType extension = file_name.Extension();
  if (!extension.empty())
    extension.erase(extension.begin());  // Erase preceding '.'.

  // Nothing to do if there's no mime type.
  if (mime_type.empty())
    return extension;

  // Nothing to do there's an extension, unless |ignore_extension| is true.
  if (!extension.empty() && !ignore_extension)
    return extension;

  // Don't do anything if there's not a preferred extension for the mime
  // type.
  base::FilePath::StringType preferred_mime_extension;
  if (!GetPreferredExtensionForMimeType(mime_type, &preferred_mime_extension))
    return extension;

  // If the existing extension is in the list of valid extensions for the
  // given type, use it. This avoids doing things like pointlessly renaming
  // "foo.jpg" to "foo.jpeg".
  std::vector<base::FilePath::StringType> all_mime_extensions;
  GetExtensionsForMimeType(mime_type, &all_mime_extensions);
  if (base::Contains(all_mime_extensions, extension))
    return extension;

  // Get the "final" extension. In most cases, this is the same as the
  // |extension|, but in cases like "foo.tar.gz", it's "gz" while
  // |extension| is "tar.gz".
  base::FilePath::StringType final_extension = file_name.FinalExtension();
  // Erase preceding '.'.
  if (!final_extension.empty())
    final_extension.erase(final_extension.begin());

  // If there's a double extension, and the second extension is in the
  // list of valid extensions for the given type, keep the double extension.
  // This avoids renaming things like "foo.tar.gz" to "foo.gz".
  if (base::Contains(all_mime_extensions, final_extension))
    return extension;
  return preferred_mime_extension;
}

}  // namespace

void SanitizeGeneratedFileName(base::FilePath::StringType* filename,
                               bool replace_trailing) {
  const base::FilePath::CharType kReplace[] = FILE_PATH_LITERAL("_");
  if (filename->empty())
    return;
  if (replace_trailing) {
    // Handle CreateFile() stripping trailing dots and spaces on filenames
    // http://support.microsoft.com/kb/115827
    size_t length = filename->size();
    size_t pos = filename->find_last_not_of(FILE_PATH_LITERAL(" ."));
    filename->resize((pos == std::string::npos) ? 0 : (pos + 1));
#if BUILDFLAG(IS_WIN)
    base::TrimWhitespace(*filename, base::TRIM_TRAILING, filename);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
    base::TrimWhitespaceASCII(*filename, base::TRIM_TRAILING, filename);
#else
#error Unsupported platform
#endif

    if (filename->empty())
      return;
    size_t trimmed = length - filename->size();
    if (trimmed)
      filename->insert(filename->end(), trimmed, kReplace[0]);
  }
  base::TrimString(*filename, FILE_PATH_LITERAL("."), filename);
  if (filename->empty())
    return;
  // Replace any path information by changing path separators.
  base::ReplaceSubstringsAfterOffset(
      filename, 0, FILE_PATH_LITERAL("/"), kReplace);
  base::ReplaceSubstringsAfterOffset(
      filename, 0, FILE_PATH_LITERAL("\\"), kReplace);
}

// Returns the filename determined from the last component of the path portion
// of the URL.  Returns an empty string if the URL doesn't have a path or is
// invalid. If the generated filename is not reliable,
// |should_overwrite_extension| will be set to true, in which case a better
// extension should be determined based on the content type.
std::string GetFileNameFromURL(const GURL& url,
                               const std::string& referrer_charset,
                               bool* should_overwrite_extension) {
  // about: and data: URLs don't have file names, but esp. data: URLs may
  // contain parts that look like ones (i.e., contain a slash).  Therefore we
  // don't attempt to divine a file name out of them.
  if (!url.is_valid() || url.SchemeIs("about") || url.SchemeIs("data"))
    return std::string();

  std::string unescaped_url_filename = base::UnescapeBinaryURLComponent(
      url.ExtractFileName(), base::UnescapeRule::NORMAL);

  // The URL's path should be escaped UTF-8, but may not be.
  std::string decoded_filename = unescaped_url_filename;
  if (!base::IsStringUTF8(decoded_filename)) {
    // TODO(jshin): this is probably not robust enough. To be sure, we need
    // encoding detection.
    std::u16string utf16_output;
    if (!referrer_charset.empty() &&
        ConvertToUTF16(unescaped_url_filename, referrer_charset.c_str(),
                       &utf16_output)) {
      decoded_filename = base::UTF16ToUTF8(utf16_output);
    } else {
      decoded_filename =
          base::WideToUTF8(base::SysNativeMBToWide(unescaped_url_filename));
    }
  }
  // If the URL contains a (possibly empty) query, assume it is a generator, and
  // allow the determined extension to be overwritten.
  *should_overwrite_extension = !decoded_filename.empty() && url.has_query();

  return decoded_filename;
}

// Returns whether the specified extension is automatically integrated into the
// windows shell.
bool IsShellIntegratedExtension(const base::FilePath::StringType& extension) {
  base::FilePath::StringType extension_lower = base::ToLowerASCII(extension);

  // .lnk files may be used to execute arbitrary code (see
  // https://nvd.nist.gov/vuln/detail/CVE-2010-2568). .local files are used by
  // Windows to determine which DLLs to load for an application.
  if ((extension_lower == FILE_PATH_LITERAL("local")) ||
      (extension_lower == FILE_PATH_LITERAL("lnk")))
    return true;

  // Setting a file's extension to a CLSID may conceal its actual file type on
  // some Windows versions (see https://nvd.nist.gov/vuln/detail/CVE-2004-0420).
  if (!extension_lower.empty() &&
      (extension_lower.front() == FILE_PATH_LITERAL('{')) &&
      (extension_lower.back() == FILE_PATH_LITERAL('}')))
    return true;
  return false;
}

// Examines the current extension in |file_name| and modifies it if necessary in
// order to ensure the filename is safe.  If |file_name| doesn't contain an
// extension or if |ignore_extension| is true, then a new extension will be
// constructed based on the |mime_type|.
//
// We're addressing two things here:
//
// 1) Usability.  If there is no reliable file extension, we want to guess a
//    reasonable file extension based on the content type.
//
// 2) Shell integration.  Some file extensions automatically integrate with the
//    shell.  We block these extensions to prevent a malicious web site from
//    integrating with the user's shell.
void EnsureSafeExtension(const std::string& mime_type,
                         bool ignore_extension,
                         base::FilePath* file_name) {
  DCHECK(file_name);
  base::FilePath::StringType extension =
      GetCorrectedExtensionUnsafe(mime_type, ignore_extension, *file_name);

#if BUILDFLAG(IS_WIN)
  const base::FilePath::CharType kDefaultExtension[] =
      FILE_PATH_LITERAL("download");

  // Rename shell-integrated extensions.
  // TODO(asanka): Consider stripping out the bad extension and replacing it
  // with the preferred extension for the MIME type if one is available.
  if (IsShellIntegratedExtension(extension))
    extension = kDefaultExtension;
#endif

  *file_name = file_name->ReplaceExtension(extension);
}

bool FilePathToString16(const base::FilePath& path, std::u16string* converted) {
#if BUILDFLAG(IS_WIN)
  converted->assign(path.value().begin(), path.value().end());
  return true;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  std::string component8 = path.AsUTF8Unsafe();
  return !component8.empty() &&
         base::UTF8ToUTF16(component8.c_str(), component8.size(), converted);
#endif
}

std::u16string GetSuggestedFilenameImpl(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_name,
    bool should_replace_extension,
    ReplaceIllegalCharactersFunction replace_illegal_characters_function) {
  // TODO: this function to be updated to match the httpbis recommendations.
  // Talk to abarth for the latest news.

  // We don't translate this fallback string, "download". If localization is
  // needed, the caller should provide localized fallback in |default_name|.
  static const base::FilePath::CharType kFinalFallbackName[] =
      FILE_PATH_LITERAL("download");
  std::string filename;  // In UTF-8
  bool overwrite_extension = false;
  bool is_name_from_content_disposition = false;
  // Try to extract a filename from content-disposition first.
  if (!content_disposition.empty()) {
    HttpContentDisposition header(content_disposition, referrer_charset);
    filename = header.filename();
    if (!filename.empty())
      is_name_from_content_disposition = true;
  }

  // Then try to use the suggested name.
  if (filename.empty() && !suggested_name.empty())
    filename = suggested_name;

  // Now try extracting the filename from the URL.  GetFileNameFromURL() only
  // looks at the last component of the URL and doesn't return the hostname as a
  // failover.
  if (filename.empty())
    filename = GetFileNameFromURL(url, referrer_charset, &overwrite_extension);

  // Finally try the URL hostname, but only if there's no default specified in
  // |default_name|.  Some schemes (e.g.: file:, about:, data:) do not have a
  // host name.
  if (filename.empty() && default_name.empty() && url.is_valid() &&
      !url.host().empty()) {
    // TODO(jungshik) : Decode a 'punycoded' IDN hostname. (bug 1264451)
    filename = url.host();
  }

  bool replace_trailing = false;
  base::FilePath::StringType result_str, default_name_str;
#if BUILDFLAG(IS_WIN)
  replace_trailing = true;
  result_str = base::UTF8ToWide(filename);
  default_name_str = base::UTF8ToWide(default_name);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  result_str = filename;
  default_name_str = default_name;
#else
#error Unsupported platform
#endif
  SanitizeGeneratedFileName(&result_str, replace_trailing);
  if (result_str.find_last_not_of(FILE_PATH_LITERAL("-_")) ==
      base::FilePath::StringType::npos) {
    result_str = !default_name_str.empty()
                     ? default_name_str
                     : base::FilePath::StringType(kFinalFallbackName);
    overwrite_extension = false;
  }
  replace_illegal_characters_function(&result_str, '_');
  base::FilePath result(result_str);
  overwrite_extension |= should_replace_extension;
  // extension should not appended to filename derived from
  // content-disposition, if it does not have one.
  // Hence mimetype and overwrite_extension values are not used.
  if (is_name_from_content_disposition)
    GenerateSafeFileName("", false, &result);
  else
    GenerateSafeFileName(mime_type, overwrite_extension, &result);

  std::u16string result16;
  if (!FilePathToString16(result, &result16)) {
    result = base::FilePath(default_name_str);
    if (!FilePathToString16(result, &result16)) {
      result = base::FilePath(kFinalFallbackName);
      FilePathToString16(result, &result16);
    }
  }
  return result16;
}

base::FilePath GenerateFileNameImpl(
    const GURL& url,
    const std::string& content_disposition,
    const std::string& referrer_charset,
    const std::string& suggested_name,
    const std::string& mime_type,
    const std::string& default_file_name,
    bool should_replace_extension,
    ReplaceIllegalCharactersFunction replace_illegal_characters_function) {
  std::u16string file_name = GetSuggestedFilenameImpl(
      url, content_disposition, referrer_charset, suggested_name, mime_type,
      default_file_name, should_replace_extension,
      replace_illegal_characters_function);

#if BUILDFLAG(IS_WIN)
  base::FilePath generated_name(base::AsWStringView(file_name));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  base::FilePath generated_name(
      base::SysWideToNativeMB(base::UTF16ToWide(file_name)));
#endif

  DCHECK(!generated_name.empty());

  return generated_name;
}

}  // namespace net
