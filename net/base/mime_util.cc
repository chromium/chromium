// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iterator>
#include <map>
#include <string>
#include <unordered_set>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "net/base/mime_util.h"
#include "net/base/platform_mime_util.h"
#include "net/http/http_util.h"

using std::string;

namespace net {

// Singleton utility class for mime types.
class MimeUtil : public PlatformMimeUtil {
 public:
  bool GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                std::string* mime_type) const;

  bool GetMimeTypeFromFile(const base::FilePath& file_path,
                           std::string* mime_type) const;

  bool GetWellKnownMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                         std::string* mime_type) const;

  bool GetPreferredExtensionForMimeType(
      const std::string& mime_type,
      base::FilePath::StringType* extension) const;

  bool MatchesMimeType(const std::string &mime_type_pattern,
                       const std::string &mime_type) const;

  bool ParseMimeTypeWithoutParameter(const std::string& type_string,
                                     std::string* top_level_type,
                                     std::string* subtype) const;

  bool IsValidTopLevelMimeType(const std::string& type_string) const;

 private:
  friend struct base::LazyInstanceTraitsBase<MimeUtil>;

  MimeUtil();

  bool GetMimeTypeFromExtensionHelper(const base::FilePath::StringType& ext,
                                      bool include_platform_types,
                                      std::string* mime_type) const;
};  // class MimeUtil

// This variable is Leaky because we need to access it from WorkerPool threads.
static base::LazyInstance<MimeUtil>::Leaky g_mime_util =
    LAZY_INSTANCE_INITIALIZER;

struct MimeInfo {
  const char* const mime_type;

  // Comma-separated list of possible extensions for the type. The first
  // extension is considered preferred.
  const char* const extensions;
};

// How to use the MIME maps
// ------------------------
// READ THIS BEFORE MODIFYING THE MIME MAPPINGS BELOW.
//
// There are two hardcoded mappings from MIME types: kPrimaryMappings and
// kSecondaryMappings.
//
// kPrimaryMappings:
//
//   Use this for mappings that are critical to the web platform.  Mappings you
//   add to this list take priority over the underlying platform when converting
//   from file extension -> MIME type.  Thus file extensions listed here will
//   work consistently across platforms.
//
// kSecondaryMappings:
//
//   Use this for mappings that must exist, but can be overridden by user
//   preferences.
//
// The following applies to both lists:
//
// * The same extension can appear multiple times in the same list under
//   different MIME types.  Extensions that appear earlier take precedence over
//   those that appear later.
//
// * A MIME type must not appear more than once in a single list.  It is valid
//   for the same MIME type to appear in kPrimaryMappings and
//   kSecondaryMappings.
//
// The MIME maps are used for three types of lookups:
//
// 1) MIME type -> file extension.  Implemented as
//    GetPreferredExtensionForMimeType().
//
//    Sources are consulted in the following order:
//
//    a) As a special case application/octet-stream is mapped to nothing.  Web
//       sites are supposed to use this MIME type to indicate that the content
//       is opaque and shouldn't be parsed as any specific type of content.  It
//       doesn't make sense to map this to anything.
//
//    b) The underlying platform.  If the operating system has a mapping from
//       the MIME type to a file extension, then that takes priority.  The
//       platform is assumed to represent the user's preference.
//
//    c) kPrimaryMappings.  Order doesn't matter since there should only be at
//       most one entry per MIME type.
//
//    d) kSecondaryMappings.  Again, order doesn't matter.
//
// 2) File extension -> MIME type.  Implemented in GetMimeTypeFromExtension().
//
//    Sources are considered in the following order:
//
//    a) kPrimaryMappings.  Order matters here since file extensions can appear
//       multiple times on these lists.  The first mapping in order of
//       appearance in the list wins.
//
//    b) Underlying platform.
//
//    c) kSecondaryMappings.  Again, the order matters.
//
// 3) File extension -> Well known MIME type.  Implemented as
//    GetWellKnownMimeTypeFromExtension().
//
//    This is similar to 2), with the exception that b) is skipped.  I.e.  Only
//    considers the hardcoded mappings in kPrimaryMappings and
//    kSecondaryMappings.

// See comments above for details on how this list is used.
static const MimeInfo kPrimaryMappings[] = {
    // Must precede audio/webm .
    {"video/webm", "webm"},

    {"application/wasm", "wasm"},
    {"application/x-chrome-extension", "crx"},
    {"application/xhtml+xml", "xhtml,xht,xhtm"},
    {"audio/flac", "flac"},
    {"audio/mp3", "mp3"},
    {"audio/ogg", "ogg,oga,opus"},
    {"audio/wav", "wav"},
    {"audio/webm", "webm"},
    {"audio/x-m4a", "m4a"},
    {"image/gif", "gif"},
    {"image/jpeg", "jpeg,jpg"},
    {"image/png", "png"},
    {"image/apng", "png"},
    {"image/webp", "webp"},
    {"multipart/related", "mht,mhtml"},
    {"text/css", "css"},
    {"text/html", "html,htm,shtml,shtm"},
    {"text/javascript", "js,mjs"},
    {"text/xml", "xml"},
    {"video/mp4", "mp4,m4v"},
    {"video/ogg", "ogv,ogm"},
};

// See comments above for details on how this list is used.
static const MimeInfo kSecondaryMappings[] = {
    // Must precede image/vnd.microsoft.icon .
    {"image/x-icon", "ico"},

    {"application/epub+zip", "epub"},
    {"application/font-woff", "woff"},
    {"application/gzip", "gz,tgz"},
    {"application/javascript", "js"},
    {"application/json", "json"},  // Per http://www.ietf.org/rfc/rfc4627.txt.
    {"application/octet-stream", "bin,exe,com"},
    {"application/pdf", "pdf"},
    {"application/pkcs7-mime", "p7m,p7c,p7z"},
    {"application/pkcs7-signature", "p7s"},
    {"application/postscript", "ps,eps,ai"},
    {"application/rdf+xml", "rdf"},
    {"application/rss+xml", "rss"},
    {"application/vnd.android.package-archive", "apk"},
    {"application/vnd.mozilla.xul+xml", "xul"},
    {"application/x-gzip", "gz,tgz"},
    {"application/x-mpegurl", "m3u8"},
    {"application/x-shockwave-flash", "swf,swl"},
    {"application/x-tar", "tar"},
    {"application/x-x509-ca-cert", "cer,crt"},
    {"application/zip", "zip"},
    {"audio/mpeg", "mp3"},
    // This is the platform mapping on recent versions of Windows 10.
    {"audio/webm", "weba"},
    {"image/bmp", "bmp"},
    {"image/jpeg", "jfif,pjpeg,pjp"},
    {"image/svg+xml", "svg,svgz"},
    {"image/tiff", "tiff,tif"},
    {"image/vnd.microsoft.icon", "ico"},
    {"image/x-png", "png"},
    {"image/x-xbitmap", "xbm"},
    {"message/rfc822", "eml"},
    {"text/calendar", "ics"},
    {"text/html", "ehtml"},
    {"text/plain", "txt,text"},
    {"text/x-sh", "sh"},
    {"text/xml", "xsl,xbl,xslt"},
    {"video/mpeg", "mpeg,mpg"},
};

// Finds mime type of |ext| from |mappings|.
template <size_t num_mappings>
static const char* FindMimeType(const MimeInfo (&mappings)[num_mappings],
                                const std::string& ext) {
  for (const auto& mapping : mappings) {
    const char* extensions = mapping.extensions;
    for (;;) {
      size_t end_pos = strcspn(extensions, ",");
      // The length check is required to prevent the StringPiece below from
      // including uninitialized memory if ext is longer than extensions.
      if (end_pos == ext.size() &&
          base::EqualsCaseInsensitiveASCII(
              base::StringPiece(extensions, ext.size()), ext)) {
        return mapping.mime_type;
      }
      extensions += end_pos;
      if (!*extensions)
        break;
      extensions += 1;  // skip over comma
    }
  }
  return nullptr;
}

static base::FilePath::StringType StringToFilePathStringType(
    const base::StringPiece& string_piece) {
#if defined(OS_WIN)
  return base::UTF8ToUTF16(string_piece);
#else
  return string_piece.as_string();
#endif
}

// Helper used in MimeUtil::GetPreferredExtensionForMimeType() to search
// preferred extension in MimeInfo arrays.
template <size_t num_mappings>
static bool FindPreferredExtension(const MimeInfo (&mappings)[num_mappings],
                                   const std::string& mime_type,
                                   base::FilePath::StringType* result) {
  // There is no preferred extension for "application/octet-stream".
  if (mime_type == "application/octet-stream")
    return false;

  for (const auto& mapping : mappings) {
    if (mapping.mime_type == mime_type) {
      const char* extensions = mapping.extensions;
      const char* extension_end = strchr(extensions, ',');
      size_t len =
          extension_end ? extension_end - extensions : strlen(extensions);
      *result = StringToFilePathStringType(base::StringPiece(extensions, len));
      return true;
    }
  }
  return false;
}

bool MimeUtil::GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                        string* result) const {
  return GetMimeTypeFromExtensionHelper(ext, true, result);
}

bool MimeUtil::GetWellKnownMimeTypeFromExtension(
    const base::FilePath::StringType& ext,
    string* result) const {
  return GetMimeTypeFromExtensionHelper(ext, false, result);
}

bool MimeUtil::GetPreferredExtensionForMimeType(
    const std::string& mime_type,
    base::FilePath::StringType* extension) const {
  // Search the MIME type in the platform DB first, then in kPrimaryMappings and
  // kSecondaryMappings.
  return GetPlatformPreferredExtensionForMimeType(mime_type, extension) ||
         FindPreferredExtension(kPrimaryMappings, mime_type, extension) ||
         FindPreferredExtension(kSecondaryMappings, mime_type, extension);
}

bool MimeUtil::GetMimeTypeFromFile(const base::FilePath& file_path,
                                   string* result) const {
  base::FilePath::StringType file_name_str = file_path.Extension();
  if (file_name_str.empty())
    return false;
  return GetMimeTypeFromExtension(file_name_str.substr(1), result);
}

bool MimeUtil::GetMimeTypeFromExtensionHelper(
    const base::FilePath::StringType& ext,
    bool include_platform_types,
    string* result) const {
  DCHECK(ext.empty() || ext[0] != '.')
      << "extension passed in must not include leading dot";

  // Avoids crash when unable to handle a long file path. See crbug.com/48733.
  const unsigned kMaxFilePathSize = 65536;
  if (ext.length() > kMaxFilePathSize)
    return false;

  // Reject a string which contains null character.
  base::FilePath::StringType::size_type nul_pos =
      ext.find(FILE_PATH_LITERAL('\0'));
  if (nul_pos != base::FilePath::StringType::npos)
    return false;

  // We implement the same algorithm as Mozilla for mapping a file extension to
  // a mime type.  That is, we first check a hard-coded list (that cannot be
  // overridden), and then if not found there, we defer to the system registry.
  // Finally, we scan a secondary hard-coded list to catch types that we can
  // deduce but that we also want to allow the OS to override.

  base::FilePath path_ext(ext);
  const string ext_narrow_str = path_ext.AsUTF8Unsafe();
  const char* mime_type = FindMimeType(kPrimaryMappings, ext_narrow_str);
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  if (include_platform_types && GetPlatformMimeTypeFromExtension(ext, result))
    return true;

  mime_type = FindMimeType(kSecondaryMappings, ext_narrow_str);
  if (mime_type) {
    *result = mime_type;
    return true;
  }

  return false;
}

MimeUtil::MimeUtil() = default;

// Tests for MIME parameter equality. Each parameter in the |mime_type_pattern|
// must be matched by a parameter in the |mime_type|. If there are no
// parameters in the pattern, the match is a success.
//
// According rfc2045 keys of parameters are case-insensitive, while values may
// or may not be case-sensitive, but they are usually case-sensitive. So, this
// function matches values in *case-sensitive* manner, however note that this
// may produce some false negatives.
bool MatchesMimeTypeParameters(const std::string& mime_type_pattern,
                               const std::string& mime_type) {
  typedef std::map<std::string, std::string> StringPairMap;

  const std::string::size_type semicolon = mime_type_pattern.find(';');
  const std::string::size_type test_semicolon = mime_type.find(';');
  if (semicolon != std::string::npos) {
    if (test_semicolon == std::string::npos)
      return false;

    base::StringPairs pattern_parameters;
    base::SplitStringIntoKeyValuePairs(mime_type_pattern.substr(semicolon + 1),
                                       '=', ';', &pattern_parameters);
    base::StringPairs test_parameters;
    base::SplitStringIntoKeyValuePairs(mime_type.substr(test_semicolon + 1),
                                       '=', ';', &test_parameters);

    // Put the parameters to maps with the keys converted to lower case.
    StringPairMap pattern_parameter_map;
    for (const auto& pair : pattern_parameters) {
      pattern_parameter_map[base::ToLowerASCII(pair.first)] = pair.second;
    }

    StringPairMap test_parameter_map;
    for (const auto& pair : test_parameters) {
      test_parameter_map[base::ToLowerASCII(pair.first)] = pair.second;
    }

    if (pattern_parameter_map.size() > test_parameter_map.size())
      return false;

    for (const auto& parameter_pair : pattern_parameter_map) {
      const auto& test_parameter_pair_it =
          test_parameter_map.find(parameter_pair.first);
      if (test_parameter_pair_it == test_parameter_map.end())
        return false;
      if (parameter_pair.second != test_parameter_pair_it->second)
        return false;
    }
  }

  return true;
}

// This comparison handles absolute maching and also basic
// wildcards.  The plugin mime types could be:
//      application/x-foo
//      application/*
//      application/*+xml
//      *
// Also tests mime parameters -- all parameters in the pattern must be present
// in the tested type for a match to succeed.
bool MimeUtil::MatchesMimeType(const std::string& mime_type_pattern,
                               const std::string& mime_type) const {
  if (mime_type_pattern.empty())
    return false;

  std::string::size_type semicolon = mime_type_pattern.find(';');
  const std::string base_pattern(mime_type_pattern.substr(0, semicolon));
  semicolon = mime_type.find(';');
  const std::string base_type(mime_type.substr(0, semicolon));

  if (base_pattern == "*" || base_pattern == "*/*")
    return MatchesMimeTypeParameters(mime_type_pattern, mime_type);

  const std::string::size_type star = base_pattern.find('*');
  if (star == std::string::npos) {
    if (base::EqualsCaseInsensitiveASCII(base_pattern, base_type))
      return MatchesMimeTypeParameters(mime_type_pattern, mime_type);
    else
      return false;
  }

  // Test length to prevent overlap between |left| and |right|.
  if (base_type.length() < base_pattern.length() - 1)
    return false;

  base::StringPiece base_pattern_piece(base_pattern);
  base::StringPiece left(base_pattern_piece.substr(0, star));
  base::StringPiece right(base_pattern_piece.substr(star + 1));

  if (!base::StartsWith(base_type, left, base::CompareCase::INSENSITIVE_ASCII))
    return false;

  if (!right.empty() &&
      !base::EndsWith(base_type, right, base::CompareCase::INSENSITIVE_ASCII))
    return false;

  return MatchesMimeTypeParameters(mime_type_pattern, mime_type);
}

// See http://www.iana.org/assignments/media-types/media-types.xhtml
static const char* const kLegalTopLevelTypes[] = {
    "application", "audio",     "example", "image", "message",
    "model",       "multipart", "text",    "video",
};

bool MimeUtil::ParseMimeTypeWithoutParameter(
    const std::string& type_string,
    std::string* top_level_type,
    std::string* subtype) const {
  std::vector<std::string> components = base::SplitString(
      type_string, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != 2)
    return false;
  TrimWhitespaceASCII(components[0], base::TRIM_LEADING, &components[0]);
  TrimWhitespaceASCII(components[1], base::TRIM_TRAILING, &components[1]);
  if (!HttpUtil::IsToken(components[0]) || !HttpUtil::IsToken(components[1]))
    return false;

  if (top_level_type)
    *top_level_type = components[0];
  if (subtype)
    *subtype = components[1];
  return true;
}

bool MimeUtil::IsValidTopLevelMimeType(const std::string& type_string) const {
  std::string lower_type = base::ToLowerASCII(type_string);
  for (const char* const legal_type : kLegalTopLevelTypes) {
    if (lower_type.compare(legal_type) == 0)
      return true;
  }

  return type_string.size() > 2 &&
         base::StartsWith(type_string, "x-",
                          base::CompareCase::INSENSITIVE_ASCII);
}

//----------------------------------------------------------------------------
// Wrappers for the singleton
//----------------------------------------------------------------------------

bool GetMimeTypeFromExtension(const base::FilePath::StringType& ext,
                              std::string* mime_type) {
  return g_mime_util.Get().GetMimeTypeFromExtension(ext, mime_type);
}

bool GetMimeTypeFromFile(const base::FilePath& file_path,
                         std::string* mime_type) {
  return g_mime_util.Get().GetMimeTypeFromFile(file_path, mime_type);
}

bool GetWellKnownMimeTypeFromExtension(const base::FilePath::StringType& ext,
                                       std::string* mime_type) {
  return g_mime_util.Get().GetWellKnownMimeTypeFromExtension(ext, mime_type);
}

bool GetPreferredExtensionForMimeType(const std::string& mime_type,
                                      base::FilePath::StringType* extension) {
  return g_mime_util.Get().GetPreferredExtensionForMimeType(mime_type,
                                                            extension);
}

bool MatchesMimeType(const std::string& mime_type_pattern,
                     const std::string& mime_type) {
  return g_mime_util.Get().MatchesMimeType(mime_type_pattern, mime_type);
}

bool ParseMimeTypeWithoutParameter(const std::string& type_string,
                                   std::string* top_level_type,
                                   std::string* subtype) {
  return g_mime_util.Get().ParseMimeTypeWithoutParameter(
      type_string, top_level_type, subtype);
}

bool IsValidTopLevelMimeType(const std::string& type_string) {
  return g_mime_util.Get().IsValidTopLevelMimeType(type_string);
}

namespace {

// From http://www.w3schools.com/media/media_mimeref.asp and
// http://plugindoc.mozdev.org/winmime.php
static const char* const kStandardImageTypes[] = {
  "image/bmp",
  "image/cis-cod",
  "image/gif",
  "image/ief",
  "image/jpeg",
  "image/webp",
  "image/pict",
  "image/pipeg",
  "image/png",
  "image/svg+xml",
  "image/tiff",
  "image/vnd.microsoft.icon",
  "image/x-cmu-raster",
  "image/x-cmx",
  "image/x-icon",
  "image/x-portable-anymap",
  "image/x-portable-bitmap",
  "image/x-portable-graymap",
  "image/x-portable-pixmap",
  "image/x-rgb",
  "image/x-xbitmap",
  "image/x-xpixmap",
  "image/x-xwindowdump"
};
static const char* const kStandardAudioTypes[] = {
  "audio/aac",
  "audio/aiff",
  "audio/amr",
  "audio/basic",
  "audio/flac",
  "audio/midi",
  "audio/mp3",
  "audio/mp4",
  "audio/mpeg",
  "audio/mpeg3",
  "audio/ogg",
  "audio/vorbis",
  "audio/wav",
  "audio/webm",
  "audio/x-m4a",
  "audio/x-ms-wma",
  "audio/vnd.rn-realaudio",
  "audio/vnd.wave"
};
static const char* const kStandardVideoTypes[] = {
  "video/avi",
  "video/divx",
  "video/flc",
  "video/mp4",
  "video/mpeg",
  "video/ogg",
  "video/quicktime",
  "video/sd-video",
  "video/webm",
  "video/x-dv",
  "video/x-m4v",
  "video/x-mpeg",
  "video/x-ms-asf",
  "video/x-ms-wmv"
};

struct StandardType {
  const char* const leading_mime_type;
  base::span<const char* const> standard_types;
};
static const StandardType kStandardTypes[] = {{"image/", kStandardImageTypes},
                                              {"audio/", kStandardAudioTypes},
                                              {"video/", kStandardVideoTypes},
                                              {nullptr, {}}};

// GetExtensionsFromHardCodedMappings() adds file extensions (without a leading
// dot) to the set |extensions|, for all MIME types matching |mime_type|.
//
// The meaning of |mime_type| depends on the value of |prefix_match|:
//
//  * If |prefix_match = false| then |mime_type| is an exact (case-insensitive)
//    string such as "text/plain".
//
//  * If |prefix_match = true| then |mime_type| is treated as the prefix for a
//    (case-insensitive) string. For instance "Text/" would match "text/plain".
void GetExtensionsFromHardCodedMappings(
    base::span<const MimeInfo> mappings,
    const std::string& mime_type,
    bool prefix_match,
    std::unordered_set<base::FilePath::StringType>* extensions) {
  for (const auto& mapping : mappings) {
    base::StringPiece cur_mime_type(mapping.mime_type);

    if (base::StartsWith(cur_mime_type, mime_type,
                         base::CompareCase::INSENSITIVE_ASCII) &&
        (prefix_match || (cur_mime_type.length() == mime_type.length()))) {
      for (const base::StringPiece& this_extension : base::SplitStringPiece(
               mapping.extensions, ",", base::TRIM_WHITESPACE,
               base::SPLIT_WANT_ALL)) {
        extensions->insert(StringToFilePathStringType(this_extension));
      }
    }
  }
}

void GetExtensionsHelper(
    base::span<const char* const> standard_types,
    const std::string& leading_mime_type,
    std::unordered_set<base::FilePath::StringType>* extensions) {
  for (auto* standard_type : standard_types) {
    g_mime_util.Get().GetPlatformExtensionsForMimeType(standard_type,
                                                       extensions);
  }

  // Also look up the extensions from hard-coded mappings in case that some
  // supported extensions are not registered in the system registry, like ogg.
  GetExtensionsFromHardCodedMappings(kPrimaryMappings, leading_mime_type, true,
                                     extensions);

  GetExtensionsFromHardCodedMappings(kSecondaryMappings, leading_mime_type,
                                     true, extensions);
}

// Note that the elements in the source set will be appended to the target
// vector.
template <class T>
void UnorderedSetToVector(std::unordered_set<T>* source,
                          std::vector<T>* target) {
  size_t old_target_size = target->size();
  target->resize(old_target_size + source->size());
  size_t i = 0;
  for (auto iter = source->begin(); iter != source->end(); ++iter, ++i)
    (*target)[old_target_size + i] = *iter;
}

// Characters to be used for mime multipart boundary.
//
// TODO(rsleevi): crbug.com/575779: Follow the spec or fix the spec.
// The RFC 2046 spec says the alphanumeric characters plus the
// following characters are legal for boundaries:  '()+_,-./:=?
// However the following characters, though legal, cause some sites
// to fail: (),./:=+
constexpr base::StringPiece kMimeBoundaryCharacters(
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

// Size of mime multipart boundary.
const size_t kMimeBoundarySize = 69;

}  // namespace

void GetExtensionsForMimeType(
    const std::string& unsafe_mime_type,
    std::vector<base::FilePath::StringType>* extensions) {
  if (unsafe_mime_type == "*/*" || unsafe_mime_type == "*")
    return;

  const std::string mime_type = base::ToLowerASCII(unsafe_mime_type);
  std::unordered_set<base::FilePath::StringType> unique_extensions;

  if (base::EndsWith(mime_type, "/*", base::CompareCase::INSENSITIVE_ASCII)) {
    std::string leading_mime_type = mime_type.substr(0, mime_type.length() - 1);

    // Find the matching StandardType from within kStandardTypes, or fall
    // through to the last (default) StandardType.
    const StandardType* type = nullptr;
    for (const StandardType& standard_type : kStandardTypes) {
      type = &standard_type;
      if (type->leading_mime_type &&
          leading_mime_type == type->leading_mime_type) {
        break;
      }
    }
    DCHECK(type);
    GetExtensionsHelper(type->standard_types,
                        leading_mime_type,
                        &unique_extensions);
  } else {
    g_mime_util.Get().GetPlatformExtensionsForMimeType(mime_type,
                                                       &unique_extensions);

    // Also look up the extensions from hard-coded mappings in case that some
    // supported extensions are not registered in the system registry, like ogg.
    GetExtensionsFromHardCodedMappings(kPrimaryMappings, mime_type, false,
                                       &unique_extensions);

    GetExtensionsFromHardCodedMappings(kSecondaryMappings, mime_type, false,
                                       &unique_extensions);
  }

  UnorderedSetToVector(&unique_extensions, extensions);
}

NET_EXPORT std::string GenerateMimeMultipartBoundary() {
  // Based on RFC 1341, section "7.2.1 Multipart: The common syntax":
  //   Because encapsulation boundaries must not appear in the body parts being
  //   encapsulated, a user agent must exercise care to choose a unique
  //   boundary. The boundary in the example above could have been the result of
  //   an algorithm designed to produce boundaries with a very low probability
  //   of already existing in the data to be encapsulated without having to
  //   prescan the data.
  //   [...]
  //   the boundary parameter [...] consists of 1 to 70 characters from a set of
  //   characters known to be very robust through email gateways, and NOT ending
  //   with white space.
  //   [...]
  //   boundary := 0*69<bchars> bcharsnospace
  //   bchars := bcharsnospace / " "
  //   bcharsnospace := DIGIT / ALPHA / "'" / "(" / ")" / "+" /
  //            "_" / "," / "-" / "." / "/" / ":" / "=" / "?"

  std::string result;
  result.reserve(kMimeBoundarySize);
  result.append("----MultipartBoundary--");
  while (result.size() < (kMimeBoundarySize - 4)) {
    char c = kMimeBoundaryCharacters[base::RandInt(
        0, kMimeBoundaryCharacters.size() - 1)];
    result.push_back(c);
  }
  result.append("----");

  // Not a strict requirement - documentation only.
  DCHECK_EQ(kMimeBoundarySize, result.size());

  return result;
}

void AddMultipartValueForUpload(const std::string& value_name,
                                const std::string& value,
                                const std::string& mime_boundary,
                                const std::string& content_type,
                                std::string* post_data) {
  DCHECK(post_data);
  // First line is the boundary.
  post_data->append("--" + mime_boundary + "\r\n");
  // Next line is the Content-disposition.
  post_data->append("Content-Disposition: form-data; name=\"" +
                    value_name + "\"\r\n");
  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that.
    post_data->append("Content-Type: " + content_type + "\r\n");
  }
  // Leave an empty line and append the value.
  post_data->append("\r\n" + value + "\r\n");
}

void AddMultipartValueForUploadWithFileName(const std::string& value_name,
                                            const std::string& file_name,
                                            const std::string& value,
                                            const std::string& mime_boundary,
                                            const std::string& content_type,
                                            std::string* post_data) {
  DCHECK(post_data);
  // First line is the boundary.
  post_data->append("--" + mime_boundary + "\r\n");
  // Next line is the Content-disposition.
  post_data->append("Content-Disposition: form-data; name=\"" + value_name +
                    "\"; filename=\"" + file_name + "\"\r\n");
  if (!content_type.empty()) {
    // If Content-type is specified, the next line is that.
    post_data->append("Content-Type: " + content_type + "\r\n");
  }
  // Leave an empty line and append the value.
  post_data->append("\r\n" + value + "\r\n");
}

void AddMultipartFinalDelimiterForUpload(const std::string& mime_boundary,
                                         std::string* post_data) {
  DCHECK(post_data);
  post_data->append("--" + mime_boundary + "--\r\n");
}

}  // namespace net
