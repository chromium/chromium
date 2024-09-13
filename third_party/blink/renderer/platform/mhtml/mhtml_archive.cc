/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/platform/mhtml/mhtml_archive.h"

#include <stddef.h>

#include "base/containers/contains.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/blink/public/mojom/loader/mhtml_load_result.mojom-blink.h"
#include "third_party/blink/renderer/platform/mhtml/archive_resource.h"
#include "third_party/blink/renderer/platform/mhtml/mhtml_parser.h"
#include "third_party/blink/renderer/platform/mhtml/serialized_resource.h"
#include "third_party/blink/renderer/platform/network/mime/mime_type_registry.h"
#include "third_party/blink/renderer/platform/text/date_components.h"
#include "third_party/blink/renderer/platform/weborigin/scheme_registry.h"
#include "third_party/blink/renderer/platform/wtf/assertions.h"
#include "third_party/blink/renderer/platform/wtf/date_math.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/ascii_ctype.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using blink::mojom::MHTMLLoadResult;

const size_t kMaximumLineLength = 76;

const char kRFC2047EncodingPrefix[] = "=?utf-8?Q?";
const size_t kRFC2047EncodingPrefixLength = 10;
const char kRFC2047EncodingSuffix[] = "?=";
const size_t kRFC2047EncodingSuffixLength = 2;

const char kQuotedPrintable[] = "quoted-printable";
const char kBase64[] = "base64";
const char kBinary[] = "binary";

// Returns the length of a line-ending if one is present starting at
// |input[index]| or zero if no line-ending is present at the given |index|.
size_t LengthOfLineEndingAtIndex(base::span<const char> input, size_t index) {
  if (input[index] == '\n')
    return 1;  // Single LF.

  if (input[index] == '\r') {
    if ((index + 1) == input.size() || input[index + 1] != '\n') {
      return 1;  // Single CR (Classic Mac OS).
    }
    return 2;    // CR-LF.
  }

  return 0;
}

// Performs quoted-printable encoding characters, per RFC 2047.
void QuotedPrintableEncode(base::span<const char> input,
                           bool is_header,
                           Vector<char>& out) {
  out.clear();
  out.reserve(base::checked_cast<wtf_size_t>(input.size()));
  if (is_header)
    out.AppendSpan(base::span_from_cstring(kRFC2047EncodingPrefix));
  size_t current_line_length = 0;
  for (size_t i = 0; i < input.size(); ++i) {
    bool is_last_character = (i == input.size() - 1);
    char current_character = input[i];
    bool requires_encoding = false;
    // All non-printable ASCII characters and = require encoding.
    if ((current_character < ' ' || current_character > '~' ||
         current_character == '=') &&
        current_character != '\t')
      requires_encoding = true;

    // Decide if space and tab characters need to be encoded.
    if (!requires_encoding &&
        (current_character == '\t' || current_character == ' ')) {
      if (is_header) {
        // White space characters should always be encoded if they appear
        // anywhere in the header.
        requires_encoding = true;
      } else {
        bool end_of_line =
            is_last_character || LengthOfLineEndingAtIndex(input, i + 1);
        requires_encoding = end_of_line;
      }
    }

    // End of line should be converted to CR-LF sequences.
    if (!is_last_character) {
      size_t length_of_line_ending = LengthOfLineEndingAtIndex(input, i);
      if (length_of_line_ending) {
        out.AppendSpan(base::span_from_cstring("\r\n"));
        current_line_length = 0;
        i += (length_of_line_ending -
              1);  // -1 because we'll ++ in the for() above.
        continue;
      }
    }

    size_t length_of_encoded_character = 1;
    if (requires_encoding)
      length_of_encoded_character += 2;
    if (!is_last_character)
      length_of_encoded_character += 1;  // + 1 for the = (soft line break).

    // Insert a soft line break if necessary.
    size_t max_line_length_for_encoded_content = kMaximumLineLength;
    if (is_header) {
      max_line_length_for_encoded_content -= kRFC2047EncodingPrefixLength;
      max_line_length_for_encoded_content -= kRFC2047EncodingSuffixLength;
    }

    if (current_line_length + length_of_encoded_character >
        max_line_length_for_encoded_content) {
      if (is_header) {
        out.AppendSpan(base::span_from_cstring(kRFC2047EncodingSuffix));
        out.AppendSpan(base::span_from_cstring("\r\n"));
        out.push_back(' ');
      } else {
        out.push_back('=');
        out.AppendSpan(base::span_from_cstring("\r\n"));
      }
      current_line_length = 0;
      if (is_header)
        out.AppendSpan(base::span_from_cstring(kRFC2047EncodingPrefix));
    }

    // Finally, insert the actual character(s).
    if (requires_encoding) {
      out.push_back('=');
      out.push_back(UpperNibbleToASCIIHexDigit(current_character));
      out.push_back(LowerNibbleToASCIIHexDigit(current_character));
      current_line_length += 3;
    } else {
      out.push_back(current_character);
      current_line_length++;
    }
  }
  if (is_header)
    out.AppendSpan(base::span_from_cstring(kRFC2047EncodingSuffix));
}

String ConvertToPrintableCharacters(const String& text) {
  // If the text contains all printable ASCII characters, no need for encoding.
  bool found_non_printable_char = false;
  for (wtf_size_t i = 0; i < text.length(); ++i) {
    if (!IsASCIIPrintable(text[i])) {
      found_non_printable_char = true;
      break;
    }
  }
  if (!found_non_printable_char)
    return text;

  // Encode the text as sequences of printable ASCII characters per RFC 2047
  // (https://tools.ietf.org/html/rfc2047). Specially, the encoded text will be
  // as:   =?utf-8?Q?encoded_text?=
  // where, "utf-8" is the chosen charset to represent the text and "Q" is the
  // Quoted-Printable format to convert to 7-bit printable ASCII characters.
  std::string utf8_text = text.Utf8();
  Vector<char> encoded_text;
  QuotedPrintableEncode(utf8_text, true /* is_header */, encoded_text);
  return String(encoded_text.data(), encoded_text.size());
}

}  // namespace

MHTMLArchive::MHTMLArchive() : load_result_(MHTMLLoadResult::kInvalidArchive) {}

// static
void MHTMLArchive::ReportLoadResult(MHTMLLoadResult result) {
  UMA_HISTOGRAM_ENUMERATION("PageSerialization.MhtmlLoading.LoadResult",
                            result);
}

// static
MHTMLArchive* MHTMLArchive::Create(const KURL& url,
                                   scoped_refptr<const SharedBuffer> data) {
  MHTMLArchive* archive = CreateArchive(url, data);
  ReportLoadResult(archive->LoadResult());
  return archive;
}

// static
MHTMLArchive* MHTMLArchive::CreateArchive(
    const KURL& url,
    scoped_refptr<const SharedBuffer> data) {
  MHTMLArchive* archive = MakeGarbageCollected<MHTMLArchive>();
  archive->archive_url_ = url;

  // |data| may be null if archive file is empty.
  if (!data || data->empty()) {
    archive->load_result_ = MHTMLLoadResult::kEmptyFile;
    return archive;
  }

  // MHTML pages can only be loaded from local URLs, http/https URLs, and
  // content URLs(Android specific).  The latter is now allowed due to full
  // sandboxing enforcement on MHTML pages.
  if (!CanLoadArchive(url)) {
    archive->load_result_ = MHTMLLoadResult::kUrlSchemeNotAllowed;
    return archive;
  }

  MHTMLParser parser(std::move(data));
  HeapVector<Member<ArchiveResource>> resources = parser.ParseArchive();
  if (resources.empty()) {
    archive->load_result_ = MHTMLLoadResult::kInvalidArchive;
    return archive;
  }

  archive->date_ = parser.CreationDate();

  size_t resources_count = resources.size();
  // The first document suitable resource is the main resource of the top frame.
  for (ArchiveResource* resource : resources) {
    if (archive->MainResource()) {
      archive->AddSubresource(resource);
      continue;
    }

    const AtomicString& mime_type = resource->MimeType();
    bool is_mime_type_suitable_for_main_resource =
        MIMETypeRegistry::IsSupportedNonImageMIMEType(mime_type);
    // Want to allow image-only MHTML archives, but retain behavior for other
    // documents that have already been created expecting the first HTML page to
    // be considered the main resource.
    if (resources_count == 1 &&
        MIMETypeRegistry::IsSupportedImageResourceMIMEType(mime_type)) {
      is_mime_type_suitable_for_main_resource = true;
    }
    // explicitly disallow JS and CSS as the main resource.
    if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(mime_type) ||
        MIMETypeRegistry::IsSupportedStyleSheetMIMEType(mime_type))
      is_mime_type_suitable_for_main_resource = false;

    if (is_mime_type_suitable_for_main_resource)
      archive->SetMainResource(resource);
    else
      archive->AddSubresource(resource);
  }
  if (archive->MainResource())
    archive->load_result_ = MHTMLLoadResult::kSuccess;
  else
    archive->load_result_ = MHTMLLoadResult::kMissingMainResource;

  return archive;
}

bool MHTMLArchive::CanLoadArchive(const KURL& url) {
  // MHTML pages can only be loaded from local URLs, http/https URLs, and
  // content URLs(Android specific).  The latter is now allowed due to full
  // sandboxing enforcement on MHTML pages.
  if (base::Contains(url::GetLocalSchemes(), url.Protocol().Ascii()))
    return true;
  if (url.ProtocolIsInHTTPFamily())
    return true;
#if BUILDFLAG(IS_ANDROID)
  if (url.ProtocolIs("content"))
    return true;
#endif
  return false;
}

void MHTMLArchive::GenerateMHTMLHeader(const String& boundary,
                                       const KURL& url,
                                       const String& title,
                                       const String& mime_type,
                                       base::Time date,
                                       Vector<char>& output_buffer) {
  DCHECK(!boundary.empty());
  DCHECK(!mime_type.empty());

  StringBuilder string_builder;
  string_builder.Append("From: <Saved by Blink>\r\n");

  // Add the document URL in the MHTML headers in order to avoid complicated
  // parsing to locate it in the multipart body headers.
  string_builder.Append("Snapshot-Content-Location: ");
  string_builder.Append(url.GetString());

  string_builder.Append("\r\nSubject: ");
  string_builder.Append(ConvertToPrintableCharacters(title));
  string_builder.Append("\r\nDate: ");
  string_builder.Append(
      // See http://tools.ietf.org/html/rfc2822#section-3.3.
      String(base::UnlocalizedTimeFormatWithPattern(date,
                                                    "E, d MMM y HH:mm:ss xx")));
  string_builder.Append("\r\nMIME-Version: 1.0\r\n");
  string_builder.Append("Content-Type: multipart/related;\r\n");
  string_builder.Append("\ttype=\"");
  string_builder.Append(mime_type);
  string_builder.Append("\";\r\n");
  string_builder.Append("\tboundary=\"");
  string_builder.Append(boundary);
  string_builder.Append("\"\r\n\r\n");

  // We use utf8() below instead of ascii() as ascii() replaces CRLFs with ??
  // (we still only have put ASCII characters in it).
  DCHECK(string_builder.ToString().ContainsOnlyASCIIOrEmpty());
  std::string utf8_string = string_builder.ToString().Utf8();

  output_buffer.AppendSpan(base::span(utf8_string));
}

void MHTMLArchive::GenerateMHTMLPart(const String& boundary,
                                     const String& content_id,
                                     EncodingPolicy encoding_policy,
                                     const SerializedResource& resource,
                                     Vector<char>& output_buffer) {
  DCHECK(!boundary.empty());
  DCHECK(content_id.empty() || content_id[0] == '<');

  StringBuilder string_builder;
  // Per the spec, the boundary must occur at the beginning of a line.
  string_builder.Append("\r\n--");
  string_builder.Append(boundary);
  string_builder.Append("\r\n");

  string_builder.Append("Content-Type: ");
  string_builder.Append(resource.mime_type);
  string_builder.Append("\r\n");

  if (!content_id.empty()) {
    string_builder.Append("Content-ID: ");
    string_builder.Append(content_id);
    string_builder.Append("\r\n");
  }

  std::string_view content_encoding;
  if (encoding_policy == kUseBinaryEncoding)
    content_encoding = kBinary;
  else if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
               resource.mime_type) ||
           MIMETypeRegistry::IsSupportedNonImageMIMEType(resource.mime_type))
    content_encoding = kQuotedPrintable;
  else
    content_encoding = kBase64;

  string_builder.Append("Content-Transfer-Encoding: ");
  string_builder.Append(content_encoding.data(), base::checked_cast<wtf_size_t>(
                                                     content_encoding.size()));
  string_builder.Append("\r\n");

  if (!resource.url.ProtocolIsAbout()) {
    string_builder.Append("Content-Location: ");
    string_builder.Append(resource.url.GetString());
    string_builder.Append("\r\n");
  }

  string_builder.Append("\r\n");

  std::string utf8_string = string_builder.ToString().Utf8();
  output_buffer.AppendSpan(base::span(utf8_string));

  if (content_encoding == kBinary) {
    for (const auto& span : *resource.data) {
      output_buffer.AppendSpan(span);
    }
  } else {
    // FIXME: ideally we would encode the content as a stream without having to
    // fetch it all.
    const SegmentedBuffer::DeprecatedFlatData flat_data(resource.data.get());
    auto data = base::span(flat_data);

    Vector<char> encoded_data;
    if (content_encoding == kQuotedPrintable) {
      QuotedPrintableEncode(data, false /* is_header */, encoded_data);
      output_buffer.AppendVector(encoded_data);
    } else {
      DCHECK_EQ(content_encoding, kBase64);
      // We are not specifying insertLFs = true below as it would cut the lines
      // with LFs and MHTML requires CRLFs.
      Base64Encode(base::as_bytes(data), encoded_data);

      auto encoded_data_span = base::span(encoded_data);
      do {
        auto [encoded_data_line, rest] = encoded_data_span.split_at(
            std::min(encoded_data_span.size(), kMaximumLineLength));
        output_buffer.AppendSpan(encoded_data_line);
        output_buffer.AppendSpan(base::span_from_cstring("\r\n"));
        encoded_data_span = rest;
      } while (!encoded_data_span.empty());
    }
  }
}

void MHTMLArchive::GenerateMHTMLFooterForTesting(const String& boundary,
                                                 Vector<char>& output_buffer) {
  DCHECK(!boundary.empty());
  std::string utf8_string = String("\r\n--" + boundary + "--\r\n").Utf8();
  output_buffer.AppendSpan(base::span(utf8_string));
}

void MHTMLArchive::SetMainResource(ArchiveResource* main_resource) {
  main_resource_ = main_resource;
}

void MHTMLArchive::AddSubresource(ArchiveResource* resource) {
  const KURL& url = resource->Url();
  subresources_.Set(url, resource);
  KURL cid_uri = MHTMLParser::ConvertContentIDToURI(resource->ContentID());
  if (cid_uri.IsValid())
    subresources_.Set(cid_uri, resource);
}

ArchiveResource* MHTMLArchive::SubresourceForURL(const KURL& url) const {
  const auto it = subresources_.find(url.GetString());
  return it != subresources_.end() ? it->value : nullptr;
}

String MHTMLArchive::GetCacheIdentifier() const {
  return archive_url_.GetString();
}

void MHTMLArchive::Trace(Visitor* visitor) const {
  visitor->Trace(main_resource_);
  visitor->Trace(subresources_);
}

}  // namespace blink
