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
//                                        URL has an GIF extension)
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
//                              Download as GIF if the URL has an GIF extension)
// * Safari 3: Download as Unknown (Note: Safari will Render as GIF if the
//                                        URL has an GIF extension)
// * Opera 9: Render as GIF
//
// Displaying as text/plain makes little sense as the content will look like
// gibberish.  Here, we could change our minds and download.
// => Chrome: Render as GIF
//
// GIF payload, Content-Type: "application/octet-stream":
// * IE 7: Render as GIF
// * Firefox 2: Download as application/octet-stream (Note: Firefox will
//                              Download as GIF if the URL has an GIF extension)
// * Safari 3: Download as Unknown (Note: Safari will Render as GIF if the
//                                        URL has an GIF extension)
// * Opera 9: Render as GIF
//
// We used to render as GIF here, but the problem is that some sites want to
// trigger downloads by sending application/octet-stream (even though they
// should be sending Content-Disposition: attachment).  Although it is safe
// to render as GIF from a security perspective, we actually get better
// compatibility if we don't sniff from application/octet stream at all.
// => Chrome: Download as application/octet-stream
//
// Note that our definition of HTML payload is much stricter than IE's
// definition and roughly the same as Firefox's definition.

#include <stdint.h>
#include <string>

#include "net/base/mime_sniffer.h"

#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "build/build_config.h"
#include "url/gurl.h"

namespace net {

// The number of content bytes we need to use all our magic numbers.  Feel free
// to increase this number if you add a longer magic number.
static const size_t kBytesRequiredForMagic = 42;

struct MagicNumber {
  const char* const mime_type;
  const std::string_view magic;
  bool is_string;
  const char* const mask;  // if set, must have same length as |magic|
};

#define MAGIC_NUMBER(mime_type, magic) \
  { (mime_type), std::string_view((magic), sizeof(magic) - 1), false, nullptr }

template <int MagicSize, int MaskSize>
class VerifySizes {
  static_assert(MagicSize == MaskSize, "sizes must be equal");

 public:
  enum { SIZES = MagicSize };
};

#define verified_sizeof(magic, mask) \
VerifySizes<sizeof(magic), sizeof(mask)>::SIZES

#define MAGIC_MASK(mime_type, magic, mask)                                    \
  {                                                                           \
    (mime_type), std::string_view((magic), verified_sizeof(magic, mask) - 1), \
        false, (mask)                                                         \
  }

// Magic strings are case insensitive and must not include '\0' characters
#define MAGIC_STRING(mime_type, magic) \
  { (mime_type), std::string_view((magic), sizeof(magic) - 1), true, nullptr }

static const MagicNumber kMagicNumbers[] = {
  // Source: HTML 5 specification
  MAGIC_NUMBER("application/pdf", "%PDF-"),
  MAGIC_NUMBER("application/postscript", "%!PS-Adobe-"),
  MAGIC_NUMBER("image/gif", "GIF87a"),
  MAGIC_NUMBER("image/gif", "GIF89a"),
  MAGIC_NUMBER("image/png", "\x89" "PNG\x0D\x0A\x1A\x0A"),
  MAGIC_NUMBER("image/jpeg", "\xFF\xD8\xFF"),
  MAGIC_NUMBER("image/bmp", "BM"),
  // Source: Mozilla
  MAGIC_NUMBER("text/plain", "#!"),  // Script
  MAGIC_NUMBER("text/plain", "%!"),  // Script, similar to PS
  MAGIC_NUMBER("text/plain", "From"),
  MAGIC_NUMBER("text/plain", ">From"),
  // Chrome specific
  MAGIC_NUMBER("application/x-gzip", "\x1F\x8B\x08"),
  MAGIC_NUMBER("audio/x-pn-realaudio", "\x2E\x52\x4D\x46"),
  MAGIC_NUMBER("video/x-ms-asf",
      "\x30\x26\xB2\x75\x8E\x66\xCF\x11\xA6\xD9\x00\xAA\x00\x62\xCE\x6C"),
  MAGIC_NUMBER("image/tiff", "I I"),
  MAGIC_NUMBER("image/tiff", "II*"),
  MAGIC_NUMBER("image/tiff", "MM\x00*"),
  MAGIC_NUMBER("audio/mpeg", "ID3"),
  MAGIC_NUMBER("image/webp", "RIFF....WEBPVP"),
  MAGIC_NUMBER("video/webm", "\x1A\x45\xDF\xA3"),
  MAGIC_NUMBER("application/zip", "PK\x03\x04"),
  MAGIC_NUMBER("application/x-rar-compressed", "Rar!\x1A\x07\x00"),
  MAGIC_NUMBER("application/x-msmetafile", "\xD7\xCD\xC6\x9A"),
  MAGIC_NUMBER("application/octet-stream", "MZ"),  // EXE
  // Sniffing for Flash:
  //
  //   MAGIC_NUMBER("application/x-shockwave-flash", "CWS"),
  //   MAGIC_NUMBER("application/x-shockwave-flash", "FLV"),
  //   MAGIC_NUMBER("application/x-shockwave-flash", "FWS"),
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
static const size_t kBytesRequiredForOfficeMagic = 8;

static const MagicNumber kOfficeMagicNumbers[] = {
  MAGIC_NUMBER("CFB", "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1"),
  MAGIC_NUMBER("OOXML", "PK\x03\x04"),
};

enum OfficeDocType {
  DOC_TYPE_WORD,
  DOC_TYPE_EXCEL,
  DOC_TYPE_POWERPOINT,
  DOC_TYPE_NONE
};

struct OfficeExtensionType {
  OfficeDocType doc_type;
  const std::string_view extension;
};

#define OFFICE_EXTENSION(type, extension) \
  { (type), std::string_view((extension), sizeof(extension) - 1) }

static const OfficeExtensionType kOfficeExtensionTypes[] = {
  OFFICE_EXTENSION(DOC_TYPE_WORD, ".doc"),
  OFFICE_EXTENSION(DOC_TYPE_EXCEL, ".xls"),
  OFFICE_EXTENSION(DOC_TYPE_POWERPOINT, ".ppt"),
  OFFICE_EXTENSION(DOC_TYPE_WORD, ".docx"),
  OFFICE_EXTENSION(DOC_TYPE_EXCEL, ".xlsx"),
  OFFICE_EXTENSION(DOC_TYPE_POWERPOINT, ".pptx"),
};

static const MagicNumber kExtraMagicNumbers[] = {
  MAGIC_NUMBER("image/x-xbitmap", "#define"),
  MAGIC_NUMBER("image/x-icon", "\x00\x00\x01\x00"),
  MAGIC_NUMBER("audio/wav", "RIFF....WAVEfmt "),
  MAGIC_NUMBER("video/avi", "RIFF....AVI LIST"),
  MAGIC_NUMBER("audio/ogg", "OggS\0"),
  MAGIC_MASK("video/mpeg", "\x00\x00\x01\xB0", "\xFF\xFF\xFF\xF0"),
  MAGIC_MASK("audio/mpeg", "\xFF\xE0", "\xFF\xE0"),
  MAGIC_NUMBER("video/3gpp", "....ftyp3g"),
  MAGIC_NUMBER("video/3gpp", "....ftypavcl"),
  MAGIC_NUMBER("video/mp4", "....ftyp"),
  MAGIC_NUMBER("video/quicktime", "....moov"),
  MAGIC_NUMBER("application/x-shockwave-flash", "CWS"),
  MAGIC_NUMBER("application/x-shockwave-flash", "FWS"),
  MAGIC_NUMBER("video/x-flv", "FLV"),
  MAGIC_NUMBER("audio/x-flac", "fLaC"),
  // Per https://tools.ietf.org/html/rfc3267#section-8.1
  MAGIC_NUMBER("audio/amr", "#!AMR\n"),

  // RAW image types.
  MAGIC_NUMBER("image/x-canon-cr2", "II\x2a\x00\x10\x00\x00\x00CR"),
  MAGIC_NUMBER("image/x-canon-crw", "II\x1a\x00\x00\x00HEAPCCDR"),
  MAGIC_NUMBER("image/x-minolta-mrw", "\x00MRM"),
  MAGIC_NUMBER("image/x-olympus-orf", "MMOR"),  // big-endian
  MAGIC_NUMBER("image/x-olympus-orf", "IIRO"),  // little-endian
  MAGIC_NUMBER("image/x-olympus-orf", "IIRS"),  // little-endian
  MAGIC_NUMBER("image/x-fuji-raf", "FUJIFILMCCD-RAW "),
  MAGIC_NUMBER("image/x-panasonic-raw",
               "IIU\x00\x08\x00\x00\x00"),  // Panasonic .raw
  MAGIC_NUMBER("image/x-panasonic-raw",
               "IIU\x00\x18\x00\x00\x00"),  // Panasonic .rw2
  MAGIC_NUMBER("image/x-phaseone-raw", "MMMMRaw"),
  MAGIC_NUMBER("image/x-x3f", "FOVb"),
};

// Our HTML sniffer differs slightly from Mozilla.  For example, Mozilla will
// decide that a document that begins "<!DOCTYPE SOAP-ENV:Envelope PUBLIC " is
// HTML, but we will not.

#define MAGIC_HTML_TAG(tag) \
  MAGIC_STRING("text/html", "<" tag)

static const MagicNumber kSniffableTags[] = {
  // XML processing directive.  Although this is not an HTML mime type, we sniff
  // for this in the HTML phase because text/xml is just as powerful as HTML and
  // we want to leverage our white space skipping technology.
  MAGIC_NUMBER("text/xml", "<?xml"),  // Mozilla
  // DOCTYPEs
  MAGIC_HTML_TAG("!DOCTYPE html"),  // HTML5 spec
  // Sniffable tags, ordered by how often they occur in sniffable documents.
  MAGIC_HTML_TAG("script"),  // HTML5 spec, Mozilla
  MAGIC_HTML_TAG("html"),  // HTML5 spec, Mozilla
  MAGIC_HTML_TAG("!--"),
  MAGIC_HTML_TAG("head"),  // HTML5 spec, Mozilla
  MAGIC_HTML_TAG("iframe"),  // Mozilla
  MAGIC_HTML_TAG("h1"),  // Mozilla
  MAGIC_HTML_TAG("div"),  // Mozilla
  MAGIC_HTML_TAG("font"),  // Mozilla
  MAGIC_HTML_TAG("table"),  // Mozilla
  MAGIC_HTML_TAG("a"),  // Mozilla
  MAGIC_HTML_TAG("style"),  // Mozilla
  MAGIC_HTML_TAG("title"),  // Mozilla
  MAGIC_HTML_TAG("b"),  // Mozilla
  MAGIC_HTML_TAG("body"),  // Mozilla
  MAGIC_HTML_TAG("br"),
  MAGIC_HTML_TAG("p"),  // Mozilla
};

// Compare content header to a magic number where magic_entry can contain '.'
// for single character of anything, allowing some bytes to be skipped.
static bool MagicCmp(std::string_view content, std::string_view magic_entry) {
  DCHECK_GE(content.length(), magic_entry.length());

  for (size_t i = 0; i < magic_entry.length(); ++i) {
    if (magic_entry[i] != '.' && magic_entry[i] != content[i])
      return false;
  }
  return true;
}

// Like MagicCmp() except that it ANDs each byte with a mask before
// the comparison, because there are some bits we don't care about.
static bool MagicMaskCmp(std::string_view content,
                         std::string_view magic_entry,
                         std::string_view magic_mask) {
  DCHECK_GE(content.length(), magic_entry.length());

  for (size_t i = 0; i < magic_entry.length(); ++i) {
    if (magic_entry[i] != '.' && magic_entry[i] != (magic_mask[i] & content[i]))
      return false;
  }
  return true;
}

static bool MatchMagicNumber(std::string_view content,
                             const MagicNumber& magic_entry,
                             std::string* result) {
  // Keep kBytesRequiredForMagic honest.
  DCHECK_LE(magic_entry.magic.length(), kBytesRequiredForMagic);

  bool match = false;
  if (content.length() >= magic_entry.magic.length()) {
    if (magic_entry.is_string) {
      // Consistency check - string entries should have no embedded nulls.
      DCHECK_EQ(std::string_view::npos, magic_entry.magic.find('\0'));

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

static bool CheckForMagicNumbers(std::string_view content,
                                 base::span<const MagicNumber> magic_numbers,
                                 std::string* result) {
  for (const MagicNumber& magic : magic_numbers) {
    if (MatchMagicNumber(content, magic, result))
      return true;
  }
  return false;
}

// Truncates |string_piece| to length |max_size| and returns true if
// |string_piece| is now exactly |max_size|.
static bool TruncateStringPiece(const size_t max_size,
                                std::string_view* string_piece) {
  // Keep kMaxBytesToSniff honest.
  DCHECK_LE(static_cast<int>(max_size), kMaxBytesToSniff);

  *string_piece = string_piece->substr(0, max_size);
  return string_piece->length() == max_size;
}

// Returns true and sets result if the content appears to be HTML.
// Clears have_enough_content if more data could possibly change the result.
static bool SniffForHTML(std::string_view content,
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
static bool SniffForMagicNumbers(std::string_view content,
                                 bool* have_enough_content,
                                 std::string* result) {
  *have_enough_content &= TruncateStringPiece(kBytesRequiredForMagic, &content);

  // Check our big table of Magic Numbers
  return CheckForMagicNumbers(content, kMagicNumbers, result);
}

// Returns true and sets result if the content matches any of
// kOfficeMagicNumbers, and the URL has the proper extension.
// Clears |have_enough_content| if more data could possibly change the result.
static bool SniffForOfficeDocs(std::string_view content,
                               const GURL& url,
                               bool* have_enough_content,
                               std::string* result) {
  *have_enough_content &=
      TruncateStringPiece(kBytesRequiredForOfficeMagic, &content);

  // Check our table of magic numbers for Office file types.
  std::string office_version;
  if (!CheckForMagicNumbers(content, kOfficeMagicNumbers, &office_version))
    return false;

  OfficeDocType type = DOC_TYPE_NONE;
  std::string_view url_path = url.path_piece();
  for (const auto& office_extension : kOfficeExtensionTypes) {
    if (base::EndsWith(url_path, office_extension.extension,
                       base::CompareCase::INSENSITIVE_ASCII)) {
      type = office_extension.doc_type;
      break;
    }
  }

  if (type == DOC_TYPE_NONE)
    return false;

  if (office_version == "CFB") {
    switch (type) {
      case DOC_TYPE_WORD:
        *result = "application/msword";
        return true;
      case DOC_TYPE_EXCEL:
        *result = "application/vnd.ms-excel";
        return true;
      case DOC_TYPE_POWERPOINT:
        *result = "application/vnd.ms-powerpoint";
        return true;
      case DOC_TYPE_NONE:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  } else if (office_version == "OOXML") {
    switch (type) {
      case DOC_TYPE_WORD:
        *result = "application/vnd.openxmlformats-officedocument."
                  "wordprocessingml.document";
        return true;
      case DOC_TYPE_EXCEL:
        *result = "application/vnd.openxmlformats-officedocument."
                  "spreadsheetml.sheet";
        return true;
      case DOC_TYPE_POWERPOINT:
        *result = "application/vnd.openxmlformats-officedocument."
                  "presentationml.presentation";
        return true;
      case DOC_TYPE_NONE:
        NOTREACHED_IN_MIGRATION();
        return false;
    }
  }

  NOTREACHED_IN_MIGRATION();
  return false;
}

static bool IsOfficeType(const std::string& type_hint) {
  return (type_hint == "application/msword" ||
          type_hint == "application/vnd.ms-excel" ||
          type_hint == "application/vnd.ms-powerpoint" ||
          type_hint == "application/vnd.openxmlformats-officedocument."
                       "wordprocessingml.document" ||
          type_hint == "application/vnd.openxmlformats-officedocument."
                       "spreadsheetml.sheet" ||
          type_hint == "application/vnd.openxmlformats-officedocument."
                       "presentationml.presentation" ||
          type_hint == "application/vnd.ms-excel.sheet.macroenabled.12" ||
          type_hint == "application/vnd.ms-word.document.macroenabled.12" ||
          type_hint == "application/vnd.ms-powerpoint.presentation."
                       "macroenabled.12" ||
          type_hint == "application/mspowerpoint" ||
          type_hint == "application/msexcel" ||
          type_hint == "application/vnd.ms-word" ||
          type_hint == "application/vnd.ms-word.document.12" ||
          type_hint == "application/vnd.msword");
}

// This function checks for files that have a Microsoft Office MIME type
// set, but are not actually Office files.
//
// If this is not actually an Office file, |*result| is set to
// "application/octet-stream", otherwise it is not modified.
//
// Returns false if additional data is required to determine the file type, or
// true if there is enough data to make a decision.
static bool SniffForInvalidOfficeDocs(std::string_view content,
                                      const GURL& url,
                                      std::string* result) {
  if (!TruncateStringPiece(kBytesRequiredForOfficeMagic, &content))
    return false;

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
static const MagicNumber kMagicXML[] = {
    MAGIC_STRING("application/atom+xml", "<feed"),
    MAGIC_STRING("application/rss+xml", "<rss"),
};

// Returns true and sets result if the content appears to contain XHTML or a
// feed.
// Clears have_enough_content if more data could possibly change the result.
//
// TODO(evanm): this is similar but more conservative than what Safari does,
// while HTML5 has a different recommendation -- what should we do?
// TODO(evanm): this is incorrect for documents whose encoding isn't a superset
// of ASCII -- do we care?
static bool SniffXML(std::string_view content,
                     bool* have_enough_content,
                     std::string* result) {
  // We allow at most 300 bytes of content before we expect the opening tag.
  *have_enough_content &= TruncateStringPiece(300, &content);

  // This loop iterates through tag-looking offsets in the file.
  // We want to skip XML processing instructions (of the form "<?xml ...")
  // and stop at the first "plain" tag, then make a decision on the mime-type
  // based on the name (or possibly attributes) of that tag.
  const int kMaxTagIterations = 5;
  size_t pos = 0;
  for (size_t i = 0; i < kMaxTagIterations && pos < content.length(); ++i) {
    pos = content.find('<', pos);
    if (pos == std::string_view::npos) {
      return false;
    }

    std::string_view current = content.substr(pos);

    // Skip XML and DOCTYPE declarations.
    static constexpr std::string_view kXmlPrefix("<?xml");
    static constexpr std::string_view kDocTypePrefix("<!DOCTYPE");
    if (base::StartsWith(current, kXmlPrefix,
                         base::CompareCase::INSENSITIVE_ASCII) ||
        base::StartsWith(current, kDocTypePrefix,
                         base::CompareCase::INSENSITIVE_ASCII)) {
      ++pos;
      continue;
    }

    if (CheckForMagicNumbers(current, kMagicXML, result))
      return true;

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
static const MagicNumber kByteOrderMark[] = {
  MAGIC_NUMBER("text/plain", "\xFE\xFF"),  // UTF-16BE
  MAGIC_NUMBER("text/plain", "\xFF\xFE"),  // UTF-16LE
  MAGIC_NUMBER("text/plain", "\xEF\xBB\xBF"),  // UTF-8
};

// Returns true and sets result to "application/octet-stream" if the content
// appears to be binary data. Otherwise, returns false and sets "text/plain".
// Clears have_enough_content if more data could possibly change the result.
static bool SniffBinary(std::string_view content,
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

static bool IsUnknownMimeType(std::string_view mime_type) {
  // TODO(tc): Maybe reuse some code in net/http/http_response_headers.* here.
  // If we do, please be careful not to alter the semantics at all.
  static const char* const kUnknownMimeTypes[] = {
    // Empty mime types are as unknown as they get.
    "",
    // The unknown/unknown type is popular and uninformative
    "unknown/unknown",
    // The second most popular unknown mime type is application/unknown
    "application/unknown",
    // Firefox rejects a mime type if it is exactly */*
    "*/*",
  };
  for (const char* const unknown_mime_type : kUnknownMimeTypes) {
    if (mime_type == unknown_mime_type)
      return true;
  }
  if (mime_type.find('/') == std::string_view::npos) {
    // Firefox rejects a mime type if it does not contain a slash
    return true;
  }
  return false;
}

// Returns true and sets result if the content appears to be a crx (Chrome
// extension) file.
// Clears have_enough_content if more data could possibly change the result.
static bool SniffCRX(std::string_view content,
                     const GURL& url,
                     bool* have_enough_content,
                     std::string* result) {
  // Technically, the crx magic number is just Cr24, but the bytes after that
  // are a version number which changes infrequently. Including it in the
  // sniffing gives us less room for error. If the version number ever changes,
  // we can just add an entry to this list.
  static const struct MagicNumber kCRXMagicNumbers[] = {
      MAGIC_NUMBER("application/x-chrome-extension", "Cr24\x02\x00\x00\x00"),
      MAGIC_NUMBER("application/x-chrome-extension", "Cr24\x03\x00\x00\x00")};

  // Only consider files that have the extension ".crx".
  if (!url.path_piece().ends_with(".crx")) {
    return false;
  }

  *have_enough_content &= TruncateStringPiece(kBytesRequiredForMagic, &content);
  return CheckForMagicNumbers(content, kCRXMagicNumbers, result);
}

bool ShouldSniffMimeType(const GURL& url, std::string_view mime_type) {
  bool sniffable_scheme = url.is_empty() || url.SchemeIsHTTPOrHTTPS() ||
#if BUILDFLAG(IS_ANDROID)
                          url.SchemeIs("content") ||
#endif
                          url.SchemeIsFile() || url.SchemeIsFileSystem();
  if (!sniffable_scheme)
    return false;

  static const char* const kSniffableTypes[] = {
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
    // Check for false Microsoft Office MIME types.
    "application/msword",
    "application/vnd.ms-excel",
    "application/vnd.ms-powerpoint",
    "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
    "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
    "application/vnd.openxmlformats-officedocument.presentationml.presentation",
    "application/vnd.ms-excel.sheet.macroenabled.12",
    "application/vnd.ms-word.document.macroenabled.12",
    "application/vnd.ms-powerpoint.presentation.macroenabled.12",
    "application/mspowerpoint",
    "application/msexcel",
    "application/vnd.ms-word",
    "application/vnd.ms-word.document.12",
    "application/vnd.msword",
  };
  for (const char* const sniffable_type : kSniffableTypes) {
    if (mime_type == sniffable_type)
      return true;
  }
  if (IsUnknownMimeType(mime_type)) {
    // The web server didn't specify a content type or specified a mime
    // type that we ignore.
    return true;
  }
  return false;
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
  // Each sniff routine may modify this if it has a better guess..
  result->assign(type_hint);

  // If the file has a Microsoft Office MIME type, we should only check that it
  // is a valid Office file.  Because this is the only reason we sniff files
  // with a Microsoft Office MIME type, we can return early.
  if (IsOfficeType(type_hint))
    return SniffForInvalidOfficeDocs(content, url, result);

  // Cache information about the type_hint
  bool hint_is_unknown_mime_type = IsUnknownMimeType(type_hint);

  // First check for HTML, unless it's a file URL and
  // |allow_sniffing_files_urls_as_html| is false.
  if (hint_is_unknown_mime_type &&
      (!url.SchemeIsFile() ||
       force_sniff_file_url_for_html == ForceSniffFileUrlsForHtml::kEnabled)) {
    // We're only willing to sniff HTML if the server has not supplied a mime
    // type, or if the type it did supply indicates that it doesn't know what
    // the type should be.
    if (SniffForHTML(content, &have_enough_content, result))
      return true;  // We succeeded in sniffing HTML.  No more content needed.
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
    if (SniffXML(content, &have_enough_content, result))
      return true;
    return have_enough_content;
  }

  // CRX files (Chrome extensions) have a special sniffing algorithm. It is
  // tighter than the others because we don't have to match legacy behavior.
  if (SniffCRX(content, url, &have_enough_content, result))
    return true;

  // Check the file extension and magic numbers to see if this is an Office
  // document.  This needs to be checked before the general magic numbers
  // because zip files and Office documents (OOXML) have the same magic number.
  if (SniffForOfficeDocs(content, url, &have_enough_content, result)) {
    return true;  // We've matched a magic number.  No more content needed.
  }

  // We're not interested in sniffing for magic numbers when the type_hint
  // is application/octet-stream.  Time to bail out.
  if (type_hint == "application/octet-stream")
    return have_enough_content;

  // Now we look in our large table of magic numbers to see if we can find
  // anything that matches the content.
  if (SniffForMagicNumbers(content, &have_enough_content, result))
    return true;  // We've matched a magic number.  No more content needed.

  return have_enough_content;
}

bool SniffMimeTypeFromLocalData(std::string_view content, std::string* result) {
  // First check the extra table.
  if (CheckForMagicNumbers(content, kExtraMagicNumbers, result))
    return true;
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
  const uint32_t kBinaryBits =
      ~(1u << '\t' | 1u << '\n' | 1u << '\r' | 1u << '\f' | 1u << '\x1b');
  for (char c : content) {
    uint8_t byte = static_cast<uint8_t>(c);
    if (byte < 0x20 && (kBinaryBits & (1u << byte)))
      return true;
  }
  return false;
}

}  // namespace net
