// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Detecting mime types is a tricky business because we need to balance
// compatibility concerns with security issues.  Here is a survey of how other
// browsers behave and then a description of how we intend to behave.
//
// HTML payload, no Content-Type header:
// * IE 7: Render as HTML
// * Firefox 2: Render as HTML
// * Safari 3: Render as HTML
// * Opera 9: Render as HTML
//
// Here the choice seems clear:
// => Chrome: Render as HTML
//
// HTML payload, Content-Type: "text/plain":
// * IE 7: Render as HTML
// * Firefox 2: Render as text
// * Safari 3: Render as text (Note: Safari will Render as HTML if the URL
//                                   has an HTML extension)
// * Opera 9: Render as text
//
// Here we choose to follow the majority (and break some compatibility with IE).
// Many folks dislike IE's behavior here.
// => Chrome: Render as text
// We generalize this as follows.  If the Content-Type header is text/plain
// we won't detect dangerous mime types (those that can execute script).
//
// HTML payload, Content-Type: "application/octet-stream":
// * IE 7: Render as HTML
// * Firefox 2: Download as application/octet-stream
// * Safari 3: Render as HTML
// * Opera 9: Render as HTML
//
// We follow Firefox.
// => Chrome: Download as application/octet-stream
// One factor in this decision is that IIS 4 and 5 will send
// application/octet-stream for .xhtml files (because they don't recognize
// the extension).  We did some experiments and it looks like this doesn't occur
// very often on the web.  We choose the more secure option.
//
// GIF payload, no Content-Type header:
// * IE 7: Render as GIF
// * Firefox 2: Render as GIF
// * Safari 3: Download as Unknown (Note: Safari will Render as GIF if the
//                                        URL has a GIF extension)
// * Opera 9: Render as GIF
//
// The choice is clear.
// => Chrome: Render as GIF
// Once we decide to render HTML without a Content-Type header, there isn't much
// reason not to render GIFs.
//
// GIF payload, Content-Type: "text/plain":
// * IE 7: Render as GIF
// * Firefox 2: Download as application/octet-stream (Note: Firefox will
//                              Download as GIF if the URL has a GIF extension)
// * Safari 3: Download as Unknown (Note: Safari will Render as GIF if the
//                                        URL has a GIF extension)
// * Opera 9: Render as GIF
//
// Displaying as text/plain makes little sense as the content will look like
// gibberish.  Here, we could change our minds and download.
// => Chrome: Render as GIF
//
// GIF payload, Content-Type: "application/octet-stream":
// * IE 7: Render as GIF
// * Firefox 2: Download as application/octet-stream (Note: Firefox will
//                              Download as GIF if the URL has a GIF extension)
// * Safari 3: Download as Unknown (Note: Safari will Render as GIF if the
//                                        URL has a GIF extension)
// * Opera 9: Render as GIF
//
// We used to render as GIF here, but the problem is that some sites want to
// trigger downloads by sending application/octet-stream (even though they
// should be sending Content-Disposition: attachment).  Although it is safe
// to render as GIF from a security perspective, we actually get better
// compatibility if we don't sniff from application/octet-stream at all.
// => Chrome: Download as application/octet-stream
//
// Note that our definition of HTML payload is much stricter than IE's
// definition and roughly the same as Firefox's definition.

#include "net/base/mime_sniffer.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "net/http/http_response_headers.h"
#include "url/gurl.h"

namespace net {

namespace {

// The number of content bytes we need to use all our magic numbers.  Feel free
// to increase this number if you add a longer magic number.
constexpr size_t kBytesRequiredForMagic = 42;

struct MagicNumber {
  const char* const mime_type;
  const std::string_view magic;
  bool is_case_sensitive = true;
  const char* mask = nullptr;  // if set, must have same length as |magic|

  template <size_t N>
  consteval MagicNumber(const char* mime_type, const char (&magic)[N])
      : mime_type(mime_type), magic(magic, N - 1) {
    CHECK(mime_type);
    // Keep kBytesRequiredForMagic honest.
    CHECK_LE(this->magic.length(), kBytesRequiredForMagic);
  }

  template <size_t N>
  static consteval MagicNumber CaseInsensitive(const char* mime_type,
                                               const char (&magic)[N]) {
    MagicNumber out(mime_type, magic);
    out.is_case_sensitive = false;
    // Consistency check - case-insensitive entries should have no embedded
    // nulls.
    CHECK(!out.magic.contains('\0'));
    return out;
  }

  template <size_t N>
  static consteval MagicNumber Masked(const char* mime_type,
                                      const char (&magic)[N],
                                      const char (&mask)[N]) {
    MagicNumber out(mime_type, magic);
    out.mask = mask;
    return out;
  }
};

constexpr MagicNumber kMagicNumbers[] = {
    // Source: HTML 5 specification
    {"application/pdf", "%PDF-"},
    {"application/postscript", "%!PS-Adobe-"},
    {"image/gif", "GIF87a"},
    {"image/gif", "GIF89a"},
    {"image/png", "\x89PNG\x0D\x0A\x1A\x0A"},
    {"image/jpeg", "\xFF\xD8\xFF"},
    {"image/bmp", "BM"},
    // Source: Mozilla
    {"text/plain", "#!"},  // Script
    {"text/plain", "%!"},  // Script, similar to PS
    {"text/plain", "From"},
    {"text/plain", ">From"},
    // Chrome specific
    {"application/x-gzip", "\x1F\x8B\x08"},
    {"audio/x-pn-realaudio", "\x2E\x52\x4D\x46"},
    {"video/x-ms-asf",
     "\x30\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C"},
    {"image/tiff", "I I"},
    {"image/tiff", "II*"},
    {"image/tiff", "MM\x00*"},
    {"audio/mpeg", "ID3"},
    {"image/webp", "RIFF....WEBPVP"},
    {"video/webm", "\x1A\x45\xDF\xA3"},
    {"application/zip", "PK\x03\x04"},
    {"application/x-rar-compressed", "Rar!\x1A\x07\x00"},
    {"application/x-msmetafile", "\xD7\xCD\xC6\x9A"},
    {"application/octet-stream", "MZ"},  // EXE

    // Sniffing for Flash:
    //
    //   {"application/x-shockwave-flash", "CWS"},
    //   {"application/x-shockwave-flash", "FLV"},
    //   {"application/x-shockwave-flash", "FWS"},
    //
    // Including these magic number for Flash is a trade off.
    //
    // Pros:
    //   * Flash is an important and popular file format
    //
    // Cons:
    //   * These patterns are fairly weak
    //   * If we mistakenly decide something is Flash, we will execute it
    //     in the origin of an unsuspecting site.  This could be a security
    //     vulnerability if the site allows users to upload content.
    //
    // On balance, we do not include these patterns.
};

// The number of content bytes we need to use all our Microsoft Office magic
// numbers.
constexpr size_t kBytesRequiredForOfficeMagic = 8;

constexpr MagicNumber kOfficeMagicNumbers[] = {
    {"CFB", "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"},
    {"OOXML", "PK\x03\x04"},
};

enum class OfficeDocType {
  kWord,
  kExcel,
  kPowerPoint,
};

struct OfficeExtensionType {
  OfficeDocType doc_type;
  std::string_view extension;
};

// clang-format off
constexpr OfficeExtensionType kOfficeExtensionTypes[] = {
    {OfficeDocType::kWord, ".doc"},
    {OfficeDocType::kExcel, ".xls"},
    {OfficeDocType::kPowerPoint, ".ppt"},
    {OfficeDocType::kWord, ".docx"},
    {OfficeDocType::kExcel, ".xlsx"},
    {OfficeDocType::kPowerPoint, ".pptx"},
};
// clang-format on

constexpr MagicNumber kExtraMagicNumbers[] = {
    {"image/x-xbitmap", "#define"},
    {"image/x-icon", "\x00\x00\x01\x00"},
    {"audio/wav", "RIFF....WAVEfmt "},
    {"video/avi", "RIFF....AVI LIST"},
    {"audio/ogg", "OggS\0"},
    MagicNumber::Masked("video/mpeg", "\x00\x00\x01\xB0", "\xFF\xFF\xFF\xF0"),
    MagicNumber::Masked("audio/mpeg", "\xFF\xE0", "\xFF\xE0"),
    {"video/3gpp", "....ftyp3g"},
    {"video/3gpp", "....ftypavcl"},
    {"video/mp4", "....ftyp"},
    {"video/quicktime", "....moov"},
    {"application/x-shockwave-flash", "CWS"},
    {"application/x-shockwave-flash", "FWS"},
    {"video/x-flv", "FLV"},
    {"audio/x-flac", "fLaC"},
    // Per https://tools.ietf.org/html/rfc3267#section-8.1
    {"audio/amr", "#!AMR\n"},

    // RAW image types.
    {"image/x-canon-cr2", "II\x2a\x00\x10\x00\x00\x00CR"},
    {"image/x-canon-crw", "II\x1a\x00\x00\x00HEAPCCDR"},
    {"image/x-minolta-mrw", "\x00MRM"},
    {"image/x-olympus-orf", "MMOR"},  // big-endian
    {"image/x-olympus-orf", "IIRO"},  // little-endian
    {"image/x-olympus-orf", "IIRS"},  // little-endian
    {"image/x-fuji-raf", "FUJIFILMCCD-RAW "},
    {"image/x-panasonic-raw", "IIU\x00\x08\x00\x00\x00"},  // Panasonic .raw
    {"image/x-panasonic-raw", "IIU\x00\x18\x00\x00\x00"},  // Panasonic .rw2
    {"image/x-phaseone-raw", "MMMMRaw"},
    {"image/x-x3f", "FOVb"},
};

// Our HTML sniffer differs slightly from Mozilla.  For example, Mozilla will
// decide that a document that begins "<!DOCTYPE SOAP-ENV:Envelope PUBLIC " is
// HTML, but we will not.

template <size_t N>
consteval MagicNumber MagicHTMLTag(const char (&magic)[N]) {
  CHECK_EQ(magic[0], '<');
  return MagicNumber::CaseInsensitive("text/html", magic);
}

constexpr MagicNumber kSniffableTags[] = {
    // XML processing directive.  Although this is not an HTML mime type, we
    // sniff for this in the HTML phase because text/xml is just as powerful as
    // HTML and we want to leverage our white space skipping technology.
    {"text/xml", "<?xml"},  // Mozilla
    // DOCTYPEs
    MagicHTMLTag("<!DOCTYPE html"),  // HTML5 spec
    // Sniffable tags, ordered by how often they occur in sniffable documents.
    MagicHTMLTag("<script"),  // HTML5 spec, Mozilla
    MagicHTMLTag("<html"),    // HTML5 spec, Mozilla
    MagicHTMLTag("<!--"),
    MagicHTMLTag("<head"),    // HTML5 spec, Mozilla
    MagicHTMLTag("<iframe"),  // Mozilla
    MagicHTMLTag("<h1"),      // Mozilla
    MagicHTMLTag("<div"),     // Mozilla
    MagicHTMLTag("<font"),    // Mozilla
    MagicHTMLTag("<table"),   // Mozilla
    MagicHTMLTag("<a"),       // Mozilla
    MagicHTMLTag("<style"),   // Mozilla
    MagicHTMLTag("<title"),   // Mozilla
    MagicHTMLTag("<b"),       // Mozilla
    MagicHTMLTag("<body"),    // Mozilla
    MagicHTMLTag("<br"),
    MagicHTMLTag("<p"),  // Mozilla
};

// Compare content header to a magic number where magic_entry can contain '.'
// for single character of anything, allowing some bytes to be skipped.
bool MagicCmp(std::string_view content, std::string_view magic_entry) {
  DCHECK_GE(content.length(), magic_entry.length());

  for (size_t i = 0; i < magic_entry.length(); ++i) {
    if (magic_entry[i] != '.' && magic_entry[i] != content[i]) {
      return false;
    }
  }
  return true;
}

// Like MagicCmp() except that it ANDs each byte with a mask before
// the comparison, because there are some bits we don't care about.
bool MagicMaskCmp(std::string_view content,
                  std::string_view magic_entry,
                  std::string_view magic_mask) {
  DCHECK_GE(content.length(), magic_entry.length());

  for (size_t i = 0; i < magic_entry.length(); ++i) {
    if (magic_entry[i] != '.' &&
        magic_entry[i] != (magic_mask[i] & content[i])) {
      return false;
    }
  }
  return true;
}

bool MatchMagicNumber(std::string_view content,
                      const MagicNumber& magic_entry,
                      std::string* result) {
  bool match = false;
  if (content.length() >= magic_entry.magic.length()) {
    if (!magic_entry.is_case_sensitive) {
      // Do a case-insensitive prefix comparison.
      match = base::StartsWith(content, magic_entry.magic,
                               base::CompareCase::INSENSITIVE_ASCII);
    } else if (!magic_entry.mask) {
      match = MagicCmp(content, magic_entry.magic);
    } else {
      std::string_view magic_mask(magic_entry.mask, magic_entry.magic.length());
      match = MagicMaskCmp(content, magic_entry.magic, magic_mask);
    }
  }

  if (match) {
    result->assign(magic_entry.mime_type);
    return true;
  }
  return false;
}

bool CheckForMagicNumbers(std::string_view content,
                          base::span<const MagicNumber> magic_numbers,
                          std::string* result) {
  for (const MagicNumber& magic : magic_numbers) {
    if (MatchMagicNumber(content, magic, result)) {
      return true;
    }
  }
  return false;
}

// Truncates |string_piece| to length |max_size| and returns true if
// |string_piece| is now exactly |max_size|.
bool TruncateStringPiece(const size_t max_size,
                         std::string_view* string_piece) {
  // Keep kMaxBytesToSniff honest.
  DCHECK_LE(static_cast<int>(max_size), kMaxBytesToSniff);

  *string_piece = string_piece->substr(0, max_size);
  return string_piece->length() == max_size;
}

// Returns true and sets result if the content appears to be HTML.
// Clears have_enough_content if more data could possibly change the result.
bool SniffForHTML(std::string_view content,
                  bool* have_enough_content,
                  std::string* result) {
  // For HTML, we are willing to consider up to 512 bytes. This may be overly
  // conservative as IE only considers 256.
  *have_enough_content &= TruncateStringPiece(512, &content);

  // We adopt a strategy similar to that used by Mozilla to sniff HTML tags,
  // but with some modifications to better match the HTML5 spec.
  std::string_view trimmed =
      base::TrimWhitespaceASCII(content, base::TRIM_LEADING);

  // |trimmed| now starts at first non-whitespace character (or is empty).
  return CheckForMagicNumbers(trimmed, kSniffableTags, result);
}

// Returns true and sets result if the content matches any of kMagicNumbers.
// Clears have_enough_content if more data could possibly change the result.
bool SniffForMagicNumbers(std::string_view content,
                          bool* have_enough_content,
                          std::string* result) {
  *have_enough_content &= TruncateStringPiece(kBytesRequiredForMagic, &content);

  // Check our big table of Magic Numbers
  return CheckForMagicNumbers(content, kMagicNumbers, result);
}

constexpr std::string_view kWordMimeType = "application/msword";
constexpr std::string_view kExcelMimeType = "application/vnd.ms-excel";
constexpr std::string_view kPowerPointMimeType =
    "application/vnd.ms-powerpoint";
constexpr std::string_view kWordOpenXmlMimeType =
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document";
constexpr std::string_view kExcelOpenXmlMimeType =
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
constexpr std::string_view kPowerPointOpenXmlMimeType =
    "application/vnd.openxmlformats-officedocument.presentationml.presentation";

// Returns true and sets result if the content matches any of
// kOfficeMagicNumbers, and the URL has the proper extension.
// Clears |have_enough_content| if more data could possibly change the result.
bool SniffForOfficeDocs(std::string_view content,
                        const GURL& url,
                        bool* have_enough_content,
                        std::string* result) {
  *have_enough_content &=
      TruncateStringPiece(kBytesRequiredForOfficeMagic, &content);

  // Check our table of magic numbers for Office file types.
  std::string office_version;
  if (!CheckForMagicNumbers(content, kOfficeMagicNumbers, &office_version)) {
    return false;
  }

  std::optional<OfficeDocType> type;
  std::string_view url_path = url.path();
  for (const auto& office_extension : kOfficeExtensionTypes) {
    if (base::EndsWith(url_path, office_extension.extension,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      type = office_extension.doc_type;
      break;
    }
  }

  if (!type) {
    return false;
  }

  if (office_version == "CFB") {
    switch (*type) {
      case OfficeDocType::kWord:
        *result = kWordMimeType;
        return true;
      case OfficeDocType::kExcel:
        *result = kExcelMimeType;
        return true;
      case OfficeDocType::kPowerPoint:
        *result = kPowerPointMimeType;
        return true;
    }
  } else if (office_version == "OOXML") {
    switch (*type) {
      case OfficeDocType::kWord:
        *result = kWordOpenXmlMimeType;
        return true;
      case OfficeDocType::kExcel:
        *result = kExcelOpenXmlMimeType;
        return true;
      case OfficeDocType::kPowerPoint:
        *result = kPowerPointOpenXmlMimeType;
        return true;
    }
  }

  NOTREACHED();
}

bool IsOfficeType(std::string_view type_hint) {
  static constexpr std::string_view kOfficeMimeTypes[] = {
      kWordMimeType,
      kExcelMimeType,
      kPowerPointMimeType,
      kWordOpenXmlMimeType,
      kExcelOpenXmlMimeType,
      kPowerPointOpenXmlMimeType,
      "application/vnd.ms-excel.sheet.macroenabled.12",
      "application/vnd.ms-word.document.macroenabled.12",
      "application/vnd.ms-powerpoint.presentation.macroenabled.12",
      "application/mspowerpoint",
      "application/msexcel",
      "application/vnd.ms-word",
      "application/vnd.ms-word.document.12",
      "application/vnd.msword",
  };
  return std::ranges::contains(kOfficeMimeTypes, type_hint);
}

// This function checks for files that have a Microsoft Office MIME type
// set, but are not actually Office files.
//
// If this is not actually an Office file, |*result| is set to
// "application/octet-stream", otherwise it is not modified.
//
// Returns false if additional data is required to determine the file type, or
// true if there is enough data to make a decision.
bool SniffForInvalidOfficeDocs(std::string_view content, std::string* result) {
  if (!TruncateStringPiece(kBytesRequiredForOfficeMagic, &content)) {
    return false;
  }

  // Check our table of magic numbers for Office file types.  If it does not
  // match one, the MIME type was invalid.  Set it instead to a safe value.
  std::string office_version;
  if (!CheckForMagicNumbers(content, kOfficeMagicNumbers, &office_version)) {
    *result = "application/octet-stream";
  }

  // We have enough information to determine if this was a Microsoft Office
  // document or not, so sniffing is completed.
  return true;
}

// Tags that indicate the content is likely XML.
constexpr MagicNumber kMagicXML[] = {
    MagicNumber::CaseInsensitive("application/atom+xml", "<feed"),
    MagicNumber::CaseInsensitive("application/rss+xml", "<rss"),
};

// Returns true and sets result if the content appears to contain XHTML or a
// feed.
// Clears have_enough_content if more data could possibly change the result.
//
// TODO(evanm): this is similar but more conservative than what Safari does,
// while HTML5 has a different recommendation -- what should we do?
// TODO(evanm): this is incorrect for documents whose encoding isn't a superset
// of ASCII -- do we care?
bool SniffXML(std::string_view content,
              bool* have_enough_content,
              std::string* result) {
  // We allow at most 300 bytes of content before we expect the opening tag.
  *have_enough_content &= TruncateStringPiece(300, &content);

  // This loop iterates through tag-looking offsets in the file.
  // We want to skip XML processing instructions (of the form "<?xml ...")
  // and stop at the first "plain" tag, then make a decision on the mime-type
  // based on the name (or possibly attributes) of that tag.
  constexpr size_t kMaxTagIterations = 5;
  size_t pos = 0;
  for (size_t i = 0; i < kMaxTagIterations && pos < content.length(); ++i) {
    pos = content.find('<', pos);
    if (pos == std::string_view::npos) {
      return false;
    }

    std::string_view current = content.substr(pos);

    // Skip XML and DOCTYPE declarations.
    if (base::StartsWith(current, "<?xml",
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::StartsWith(current, "<!DOCTYPE",
                         base::CompareCase::INSENSITIVE_ASCII)) {
      ++pos;
      continue;
    }

    if (CheckForMagicNumbers(current, kMagicXML, result)) {
      return true;
    }

    // TODO(evanm): handle RSS 1.0, which is an RDF format and more difficult
    // to identify.

    // If we get here, we've hit an initial tag that hasn't matched one of the
    // above tests.  Abort.
    return true;
  }

  // We iterated too far without finding a start tag.
  // If we have more content to look at, we aren't going to change our mind by
  // seeing more bytes from the network.
  return pos < content.length();
}

// Byte order marks
constexpr MagicNumber kByteOrderMark[] = {
    {"text/plain", "\xFE\xFF"},      // UTF-16BE
    {"text/plain", "\xFF\xFE"},      // UTF-16LE
    {"text/plain", "\xEF\xBB\xBF"},  // UTF-8
};

// Returns true and sets result to "application/octet-stream" if the content
// appears to be binary data. Otherwise, returns false and sets "text/plain".
// Clears have_enough_content if more data could possibly change the result.
bool SniffBinary(std::string_view content,
                 bool* have_enough_content,
                 std::string* result) {
  // There is no consensus about exactly how to sniff for binary content.
  // * IE 7: Don't sniff for binary looking bytes, but trust the file extension.
  // * Firefox 3.5: Sniff first 4096 bytes for a binary looking byte.
  // Here, we side with FF, but with a smaller buffer. This size was chosen
  // because it is small enough to comfortably fit into a single packet (after
  // allowing for headers) and yet large enough to account for binary formats
  // that have a significant amount of ASCII at the beginning (crbug.com/15314).
  const bool is_truncated = TruncateStringPiece(kMaxBytesToSniff, &content);

  // First, we look for a BOM.
  std::string unused;
  if (CheckForMagicNumbers(content, kByteOrderMark, &unused)) {
    // If there is BOM, we think the buffer is not binary.
    result->assign("text/plain");
    return false;
  }

  // Next we look to see if any of the bytes "look binary."
  if (LooksLikeBinary(content)) {
    result->assign("application/octet-stream");
    return true;
  }

  // No evidence either way. Default to non-binary and, if truncated, clear
  // have_enough_content because there could be a binary looking byte in the
  // truncated data.
  *have_enough_content &= is_truncated;
  result->assign("text/plain");
  return false;
}

bool IsUnknownMimeType(std::string_view mime_type) {
  // TODO(tc): Maybe reuse some code in net/http/http_response_headers.* here.
  // If we do, please be careful not to alter the semantics at all.
  static constexpr std::string_view kUnknownMimeTypes[] = {
      // Empty mime types are as unknown as they get.
      "",
      // The unknown/unknown type is popular and uninformative
      "unknown/unknown",
      // The second most popular unknown mime type is application/unknown
      "application/unknown",
      // Firefox rejects a mime type if it is exactly */*
      "*/*",
  };
  return std::ranges::contains(kUnknownMimeTypes, mime_type) ||
         // Firefox rejects a mime type if it does not contain a slash
         !mime_type.contains('/');
}

// Returns true and sets result if the content appears to be a crx (Chrome
// extension) file.
// Clears have_enough_content if more data could possibly change the result.
bool SniffCRX(std::string_view content,
              const GURL& url,
              bool* have_enough_content,
              std::string* result) {
  // Technically, the crx magic number is just Cr24, but the bytes after that
  // are a version number which changes infrequently. Including it in the
  // sniffing gives us less room for error. If the version number ever changes,
  // we can just add an entry to this list.
  static constexpr MagicNumber kCRXMagicNumbers[] = {
      {"application/x-chrome-extension", "Cr24\x02\x00\x00\x00"},
      {"application/x-chrome-extension", "Cr24\x03\x00\x00\x00"},
  };

  // Only consider files that have the extension ".crx".
  if (!url.path().ends_with(".crx")) {
    return false;
  }

  *have_enough_content &= TruncateStringPiece(kBytesRequiredForMagic, &content);
  return CheckForMagicNumbers(content, kCRXMagicNumbers, result);
}

}  // namespace

bool ShouldSniffMimeType(const GURL& url,
                         const HttpResponseHeaders* http_response_headers,
                         std::string_view mime_type) {
  bool sniffable_scheme = url.is_empty() || url.SchemeIsHTTPOrHTTPS() ||
#if BUILDFLAG(IS_ANDROID)
                          url.SchemeIs("content") ||
#endif
                          url.SchemeIsFile() || url.SchemeIsFileSystem();
  if (!sniffable_scheme) {
    return false;
  }

  // If the "x-content-type-options" header is "nosniff", do not sniff. Only the
  // first matching header is checked, per the fetch spec.
  if (http_response_headers) {
    std::optional<std::string_view> header =
        http_response_headers->EnumerateHeader(/*iter=*/nullptr,
                                               "x-content-type-options");
    if (header && base::EqualsCaseInsensitiveASCII(*header, "nosniff")) {
      return false;
    }
  }

  static constexpr std::string_view kSniffableTypes[] = {
      // Many web servers are misconfigured to send text/plain for many
      // different types of content.
      "text/plain",
      // We want to sniff application/octet-stream for
      // application/x-chrome-extension, but nothing else.
      "application/octet-stream",
      // XHTML and Atom/RSS feeds are often served as plain xml instead of
      // their more specific mime types.
      "text/xml",
      "application/xml",
  };
  return std::ranges::contains(kSniffableTypes, mime_type) ||
         // Check for false Microsoft Office MIME types.
         IsOfficeType(mime_type) ||
         // The web server didn't specify a content type or specified a mime
         // type that we ignore.
         IsUnknownMimeType(mime_type);
}

bool SniffMimeType(std::string_view content,
                   const GURL& url,
                   const std::string& type_hint,
                   ForceSniffFileUrlsForHtml force_sniff_file_url_for_html,
                   std::string* result) {
  // Sanity check.
  DCHECK_LT(content.length(), 1000000U);
  DCHECK(result);

  // By default, we assume we have enough content.
  // Each sniff routine may unset this if it wasn't provided enough content.
  bool have_enough_content = true;

  // By default, we'll return the type hint.
  // Each sniff routine may modify this if it has a better guess.
  result->assign(type_hint);

  // If the file has a Microsoft Office MIME type, we should only check that it
  // is a valid Office file.  Because this is the only reason we sniff files
  // with a Microsoft Office MIME type, we can return early.
  if (IsOfficeType(type_hint)) {
    return SniffForInvalidOfficeDocs(content, result);
  }

  // Cache information about the type_hint
  const bool hint_is_unknown_mime_type = IsUnknownMimeType(type_hint);

  // First check for HTML, unless it's a file URL and
  // |force_sniff_file_url_for_html| is kDisabled.
  if (hint_is_unknown_mime_type &&
      (!url.SchemeIsFile() ||
       force_sniff_file_url_for_html == ForceSniffFileUrlsForHtml::kEnabled)) {
    // We're only willing to sniff HTML if the server has not supplied a mime
    // type, or if the type it did supply indicates that it doesn't know what
    // the type should be.
    if (SniffForHTML(content, &have_enough_content, result)) {
      return true;  // We succeeded in sniffing HTML.  No more content needed.
    }
  }

  // We're only willing to sniff for binary in 3 cases:
  // 1. The server has not supplied a mime type.
  // 2. The type it did supply indicates that it doesn't know what the type
  //    should be.
  // 3. The type is "text/plain" which is the default on some web servers and
  //    could be indicative of a mis-configuration that we shield the user from.
  const bool hint_is_text_plain = (type_hint == "text/plain");
  if (hint_is_unknown_mime_type || hint_is_text_plain) {
    if (!SniffBinary(content, &have_enough_content, result)) {
      // If the server said the content was text/plain and it doesn't appear
      // to be binary, then we trust it.
      if (hint_is_text_plain) {
        return have_enough_content;
      }
    }
  }

  // If we have plain XML, sniff XML subtypes.
  if (type_hint == "text/xml" || type_hint == "application/xml") {
    // We're not interested in sniffing these types for images and the like.
    // Instead, we're looking explicitly for a feed.  If we don't find one
    // we're done and return early.
    if (SniffXML(content, &have_enough_content, result)) {
      return true;
    }
    return have_enough_content;
  }

  // CRX files (Chrome extensions) have a special sniffing algorithm. It is
  // tighter than the others because we don't have to match legacy behavior.
  if (SniffCRX(content, url, &have_enough_content, result)) {
    return true;
  }

  // Check the file extension and magic numbers to see if this is an Office
  // document.  This needs to be checked before the general magic numbers
  // because zip files and Office documents (OOXML) have the same magic number.
  if (SniffForOfficeDocs(content, url, &have_enough_content, result)) {
    return true;  // We've matched a magic number.  No more content needed.
  }

  // We're not interested in sniffing for magic numbers when the type_hint
  // is application/octet-stream.  Time to bail out.
  if (type_hint == "application/octet-stream") {
    return have_enough_content;
  }

  // Now we look in our large table of magic numbers to see if we can find
  // anything that matches the content.
  if (SniffForMagicNumbers(content, &have_enough_content, result)) {
    return true;  // We've matched a magic number.  No more content needed.
  }

  return have_enough_content;
}

bool SniffMimeTypeFromLocalData(std::string_view content, std::string* result) {
  // First check the extra table.
  if (CheckForMagicNumbers(content, kExtraMagicNumbers, result)) {
    return true;
  }
  // Finally check the original table.
  return CheckForMagicNumbers(content, kMagicNumbers, result);
}

bool LooksLikeBinary(std::string_view content) {
  // The definition of "binary bytes" is from the spec at
  // https://mimesniff.spec.whatwg.org/#binary-data-byte
  //
  // The bytes which are considered to be "binary" are all < 0x20. Encode them
  // one bit per byte, with 1 for a "binary" bit, and 0 for a "text" bit. The
  // least-significant bit represents byte 0x00, the most-significant bit
  // represents byte 0x1F.
  constexpr uint32_t kBinaryBits =
      ~(1u << '\t' | 1u << '\n' | 1u << '\r' | 1u << '\f' | 1u << '\x1b');
  for (char c : content) {
    uint8_t byte = static_cast<uint8_t>(c);
    if (byte < 0x20 && (kBinaryBits & (1u << byte))) {
      return true;
    }
  }
  return false;
}

}  // namespace net
