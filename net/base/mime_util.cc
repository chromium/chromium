// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/mime_util.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include "base/base64.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/lazy_instance.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
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
      std::string_view mime_type,
      base::FilePath::StringType* extension) const;

  bool MatchesMimeType(std::string_view mime_type_pattern,
                       std::string_view mime_type) const;

  bool ParseMimeTypeWithoutParameter(std::string_view type_string,
                                     std::string* top_level_type,
                                     std::string* subtype) const;

  bool IsValidTopLevelMimeType(std::string_view type_string) const;

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
  const std::string_view mime_type;

  // Comma-separated list of possible extensions for the type. The first
  // extension is considered preferred.
  const std::string_view extensions;
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

    // Must precede audio/mp3
    {"audio/mpeg", "mp3"},

    {"application/wasm", "wasm"},
    {"application/x-chrome-extension", "crx"},
    {"application/xhtml+xml", "xhtml,xht,xhtm"},
    {"audio/flac", "flac"},
    {"audio/mp3", "mp3"},
    {"audio/ogg", "ogg,oga,opus"},
    {"audio/wav", "wav"},
    {"audio/webm", "webm"},
    {"audio/x-m4a", "m4a"},
    {"image/avif", "avif"},
    {"image/gif", "gif"},
    {"image/jpeg", "jpeg,jpg"},
    {"image/png", "png"},
    {"image/apng", "png,apng"},
    {"image/svg+xml", "svg,svgz"},
    {"image/webp", "webp"},
    {"multipart/related", "mht,mhtml"},
    {"text/css", "css"},
    {"text/html", "html,htm,shtml,shtm"},
    {"text/javascript", "js,mjs"},
    {"text/xml", "xml"},
    {"video/mp4", "mp4,m4v"},
    {"video/ogg", "ogv,ogm"},

    // This is a primary mapping (overrides the platform) rather than secondary
    // to work around an issue when Excel is installed on Windows. Excel
    // registers csv as application/vnd.ms-excel instead of text/csv from RFC
    // 4180. See https://crbug.com/139105.
    {"text/csv", "csv"},
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
    {"application/msword", "doc,dot"},
    {"application/octet-stream", "bin,exe,com"},
    {"application/pdf", "pdf"},
    {"application/pkcs7-mime", "p7m,p7c,p7z"},
    {"application/pkcs7-signature", "p7s"},
    {"application/postscript", "ps,eps,ai"},
    {"application/rdf+xml", "rdf"},
    {"application/rss+xml", "rss"},
    {"application/rtf", "rtf"},
    {"application/vnd.android.package-archive", "apk"},
    {"application/vnd.mozilla.xul+xml", "xul"},
    {"application/vnd.ms-excel", "xls"},
    {"application/vnd.ms-powerpoint", "ppt"},
    {"application/"
     "vnd.openxmlformats-officedocument.presentationml.presentation",
     "pptx"},
    {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
     "xlsx"},
    {"application/vnd.openxmlformats-officedocument.wordprocessingml.document",
     "docx"},
    {"application/x-gzip", "gz,tgz"},
    {"application/x-mpegurl", "m3u8"},
    {"application/x-shockwave-flash", "swf,swl"},
    {"application/x-tar", "tar"},
    {"application/x-x509-ca-cert", "cer,crt"},
    {"application/zip", "zip"},
    // This is the platform mapping on recent versions of Windows 10.
    {"audio/webm", "weba"},
    {"image/bmp", "bmp"},
    {"image/jpeg", "jfif,pjpeg,pjp"},
    {"image/tiff", "tiff,tif"},
    {"image/vnd.microsoft.icon", "ico"},
    {"image/x-png", "png"},
    {"image/x-xbitmap", "xbm"},
    {"message/rfc822", "eml"},
    {"text/calendar", "ics"},
    {"text/html", "ehtml"},
    {"text/plain", "txt,text"},
    {"text/vtt", "vtt"},
    {"text/x-sh", "sh"},
    {"text/xml", "xsl,xbl,xslt"},
    {"video/mpeg", "mpeg,mpg"},
};

// Finds mime type of |ext| from |mappings|.
template <size_t num_mappings>
static std::optional<std::string_view> FindMimeType(
    const MimeInfo (&mappings)[num_mappings],
    const std::string& ext) {
  for (const auto& mapping : mappings) {
    for (std::string_view extension :
         base::SplitStringPiece(mapping.extensions, ",", base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_ALL)) {
      if (base::EqualsCaseInsensitiveASCII(extension, ext)) {
        return mapping.mime_type;
      }
    }
  }
  return std::nullopt;
}

static base::FilePath::StringType StringToFilePathStringType(
    std::string_view string_piece) {
#if BUILDFLAG(IS_WIN)
  return base::UTF8ToWide(string_piece);
#else
  return std::string(string_piece);
#endif
}

// Helper used in MimeUtil::GetPreferredExtensionForMimeType() to search
// preferred extension in MimeInfo arrays.
template <size_t num_mappings>
static bool FindPreferredExtension(const MimeInfo (&mappings)[num_mappings],
                                   std::string_view mime_type,
                                   base::FilePath::StringType* result) {
  // There is no preferred extension for "application/octet-stream".
  if (mime_type == "application/octet-stream")
    return false;

  for (const auto& mapping : mappings) {
    if (mapping.mime_type == mime_type) {
      const size_t pos = mapping.extensions.find(',');
      *result = StringToFilePathStringType(mapping.extensions.substr(0, pos));
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
    std::string_view mime_type,
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
  std::optional<std::string_view> mime_type =
      FindMimeType(kPrimaryMappings, ext_narrow_str);
  if (mime_type) {
    *result = mime_type.value();
    return true;
  }

  if (include_platform_types && GetPlatformMimeTypeFromExtension(ext, result))
    return true;

  mime_type = FindMimeType(kSecondaryMappings, ext_narrow_str);
  if (mime_type) {
    *result = mime_type.value();
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
bool MatchesMimeTypeParameters(std::string_view mime_type_pattern,
                               std::string_view mime_type) {
  typedef std::map<std::string, std::string> StringPairMap;

  const std::string_view::size_type semicolon = mime_type_pattern.find(';');
  const std::string_view::size_type test_semicolon = mime_type.find(';');
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
bool MimeUtil::MatchesMimeType(std::string_view mime_type_pattern,
                               std::string_view mime_type) const {
  if (mime_type_pattern.empty())
    return false;

  std::string_view::size_type semicolon = mime_type_pattern.find(';');
  const std::string_view base_pattern = mime_type_pattern.substr(0, semicolon);
  semicolon = mime_type.find(';');
  const std::string_view base_type = mime_type.substr(0, semicolon);

  if (base_pattern == "*" || base_pattern == "*/*")
    return MatchesMimeTypeParameters(mime_type_pattern, mime_type);

  const std::string_view::size_type star = base_pattern.find('*');
  if (star == std::string::npos) {
    if (base::EqualsCaseInsensitiveASCII(base_pattern, base_type))
      return MatchesMimeTypeParameters(mime_type_pattern, mime_type);
    else
      return false;
  }

  // Test length to prevent overlap between |left| and |right|.
  if (base_type.length() < base_pattern.length() - 1)
    return false;

  std::string_view base_pattern_piece(base_pattern);
  std::string_view left(base_pattern_piece.substr(0, star));
  std::string_view right(base_pattern_piece.substr(star + 1));

  if (!base::StartsWith(base_type, left, base::CompareCase::INSENSITIVE_ASCII))
    return false;

  if (!right.empty() &&
      !base::EndsWith(base_type, right, base::CompareCase::INSENSITIVE_ASCII))
    return false;

  return MatchesMimeTypeParameters(mime_type_pattern, mime_type);
}

bool ParseMimeType(std::string_view type_str,
                   std::string* mime_type,
                   base::StringPairs* params) {
  // Trim leading and trailing whitespace from type.  We include '(' in
  // the trailing trim set to catch media-type comments, which are not at all
  // standard, but may occur in rare cases.
  size_t type_val = type_str.find_first_not_of(HTTP_LWS);
  type_val = std::min(type_val, type_str.length());
  size_t type_end = type_str.find_first_of(HTTP_LWS ";(", type_val);
  if (type_end == std::string::npos)
    type_end = type_str.length();

  // Reject a mime-type if it does not include a slash.
  size_t slash_pos = type_str.find_first_of('/');
  if (slash_pos == std::string::npos || slash_pos > type_end)
    return false;
  if (mime_type)
    *mime_type = type_str.substr(type_val, type_end - type_val);

  // Iterate over parameters. Can't split the string around semicolons
  // preemptively because quoted strings may include semicolons. Mostly matches
  // logic in https://mimesniff.spec.whatwg.org/. Main differences: Does not
  // validate characters are HTTP token code points / HTTP quoted-string token
  // code points, and ignores spaces after "=" in parameters.
  if (params)
    params->clear();
  std::string::size_type offset = type_str.find_first_of(';', type_end);
  while (offset < type_str.size()) {
    DCHECK_EQ(';', type_str[offset]);
    // Trim off the semicolon.
    ++offset;

    // Trim off any following spaces.
    offset = type_str.find_first_not_of(HTTP_LWS, offset);
    std::string::size_type param_name_start = offset;

    // Extend parameter name until run into a semicolon or equals sign.  Per
    // spec, trailing spaces are not removed.
    offset = type_str.find_first_of(";=", offset);

    // Nothing more to do if at end of string, or if there's no parameter
    // value, since names without values aren't allowed.
    if (offset == std::string::npos || type_str[offset] == ';')
      continue;

    auto param_name = base::MakeStringPiece(type_str.begin() + param_name_start,
                                            type_str.begin() + offset);

    // Now parse the value.
    DCHECK_EQ('=', type_str[offset]);
    // Trim off the '='.
    offset++;

    // Remove leading spaces. This violates the spec, though it matches
    // pre-existing behavior.
    //
    // TODO(mmenke): Consider doing this (only?) after parsing quotes, which
    // seems to align more with the spec - not the content-type spec, but the
    // GET spec's way of getting an encoding, and the spec for handling
    // boundary values as well.
    // See https://encoding.spec.whatwg.org/#names-and-labels.
    offset = type_str.find_first_not_of(HTTP_LWS, offset);

    std::string param_value;
    if (offset == std::string::npos || type_str[offset] == ';') {
      // Nothing to do here - an unquoted string of only whitespace should be
      // skipped.
      continue;
    } else if (type_str[offset] != '"') {
      // If the first character is not a quotation mark, copy data directly.
      std::string::size_type value_start = offset;
      offset = type_str.find_first_of(';', offset);
      std::string::size_type value_end = offset;

      // Remove terminal whitespace. If ran off the end of the string, have to
      // update |value_end| first.
      if (value_end == std::string::npos)
        value_end = type_str.size();
      while (value_end > value_start &&
             HttpUtil::IsLWS(type_str[value_end - 1])) {
        --value_end;
      }

      param_value = type_str.substr(value_start, value_end - value_start);
    } else {
      // Otherwise, append data, with special handling for backslashes, until
      // a close quote.  Do not trim whitespace for quoted-string.

      // Skip open quote.
      DCHECK_EQ('"', type_str[offset]);
      ++offset;

      while (offset < type_str.size() && type_str[offset] != '"') {
        // Skip over backslash and append the next character, when not at
        // the end of the string. Otherwise, copy the next character (Which may
        // be a backslash).
        if (type_str[offset] == '\\' && offset + 1 < type_str.size()) {
          ++offset;
        }
        param_value += type_str[offset];
        ++offset;
      }

      offset = type_str.find_first_of(';', offset);
    }
    if (params)
      params->emplace_back(param_name, param_value);
  }
  return true;
}

bool MimeUtil::ParseMimeTypeWithoutParameter(std::string_view type_string,
                                             std::string* top_level_type,
                                             std::string* subtype) const {
  std::vector<std::string_view> components = base::SplitStringPiece(
      type_string, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  if (components.size() != 2)
    return false;
  components[0] = TrimWhitespaceASCII(components[0], base::TRIM_LEADING);
  components[1] = TrimWhitespaceASCII(components[1], base::TRIM_TRAILING);
  if (!HttpUtil::IsToken(components[0]) || !HttpUtil::IsToken(components[1]))
    return false;

  if (top_level_type)
    top_level_type->assign(std::string(components[0]));

  if (subtype)
    subtype->assign(std::string(components[1]));

  return true;
}

// See https://www.iana.org/assignments/media-types/media-types.xhtml
static const char* const kLegalTopLevelTypes[] = {
    "application", "audio", "example",   "font", "image",
    "message",     "model", "multipart", "text", "video",
};

bool MimeUtil::IsValidTopLevelMimeType(std::string_view type_string) const {
  std::string lower_type = base::ToLowerASCII(type_string);
  for (const char* const legal_type : kLegalTopLevelTypes) {
    if (lower_type.compare(legal_type) == 0) {
      return true;
    }
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

bool GetPreferredExtensionForMimeType(std::string_view mime_type,
                                      base::FilePath::StringType* extension) {
  return g_mime_util.Get().GetPreferredExtensionForMimeType(mime_type,
                                                            extension);
}

bool MatchesMimeType(std::string_view mime_type_pattern,
                     std::string_view mime_type) {
  return g_mime_util.Get().MatchesMimeType(mime_type_pattern, mime_type);
}

bool ParseMimeTypeWithoutParameter(std::string_view type_string,
                                   std::string* top_level_type,
                                   std::string* subtype) {
  return g_mime_util.Get().ParseMimeTypeWithoutParameter(
      type_string, top_level_type, subtype);
}

bool IsValidTopLevelMimeType(std::string_view type_string) {
  return g_mime_util.Get().IsValidTopLevelMimeType(type_string);
}

namespace {

// From http://www.w3schools.com/media/media_mimeref.asp and
// http://plugindoc.mozdev.org/winmime.php
static const char* const kStandardImageTypes[] = {"image/avif",
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
                                                  "image/x-xwindowdump"};
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
// https://tools.ietf.org/html/rfc8081
static const char* const kStandardFontTypes[] = {
    "font/collection", "font/otf",  "font/sfnt",
    "font/ttf",        "font/woff", "font/woff2",
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
                                              {"font/", kStandardFontTypes},
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
    std::string_view cur_mime_type(mapping.mime_type);

    if (base::StartsWith(cur_mime_type, mime_type,
                         base::CompareCase::INSENSITIVE_ASCII) &&
        (prefix_match || (cur_mime_type.length() == mime_type.length()))) {
      for (std::string_view this_extension : base::SplitStringPiece(
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
constexpr std::string_view kMimeBoundaryCharacters(
    "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

// Size of mime multipart boundary.
const size_t kMimeBoundarySize = 69;

}  // namespace

void GetExtensionsForMimeType(
    std::string_view unsafe_mime_type,
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

// TODO(toyoshim): We may prefer to implement a strict RFC2616 media-type
// (https://tools.ietf.org/html/rfc2616#section-3.7) parser.
std::optional<std::string> ExtractMimeTypeFromMediaType(
    std::string_view type_string,
    bool accept_comma_separated) {
  std::string::size_type end = type_string.find(';');
  if (accept_comma_separated) {
    end = std::min(end, type_string.find(','));
  }
  std::string top_level_type;
  std::string subtype;
  if (ParseMimeTypeWithoutParameter(type_string.substr(0, end), &top_level_type,
                                    &subtype)) {
    return top_level_type + "/" + subtype;
  }
  return std::nullopt;
}

}  // namespace net
