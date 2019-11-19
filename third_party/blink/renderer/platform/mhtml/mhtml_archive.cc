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
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"
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

const wtf_size_t kMaximumLineLength = 76;

const char kRFC2047EncodingPrefix[] = "=?utf-8?Q?";
const size_t kRFC2047EncodingPrefixLength = 10;
const char kRFC2047EncodingSuffix[] = "?=";
const size_t kRFC2047EncodingSuffixLength = 2;

const char kQuotedPrintable[] = "quoted-printable";
const char kBase64[] = "base64";
const char kBinary[] = "binary";

// Returns the length of a line-ending if one is present starting at
// |input[index]| or zero if no line-ending is present at the given |index|.
size_t LengthOfLineEndingAtIndex(const char* input,
                                 size_t input_length,
                                 size_t index) {
  SECURITY_DCHECK(index < input_length);
  if (input[index] == '\n')
    return 1;  // Single LF.

  if (input[index] == '\r') {
    if ((index + 1) == input_length || input[index + 1] != '\n')
      return 1;  // Single CR (Classic Mac OS).
    return 2;    // CR-LF.
  }

  return 0;
}

// Performs quoted-printable encoding characters, per RFC 2047.
void QuotedPrintableEncode(const char* input,
                           wtf_size_t input_length,
                           bool is_header,
                           Vector<char>& out) {
  out.clear();
  out.ReserveCapacity(input_length);
  if (is_header)
    out.Append(kRFC2047EncodingPrefix, kRFC2047EncodingPrefixLength);
  size_t current_line_length = 0;
  for (size_t i = 0; i < input_length; ++i) {
    bool is_last_character = (i == input_length - 1);
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
        bool end_of_line = is_last_character || LengthOfLineEndingAtIndex(
                                                    input, input_length, i + 1);
        requires_encoding = end_of_line;
      }
    }

    // End of line should be converted to CR-LF sequences.
    if (!is_last_character) {
      size_t length_of_line_ending =
          LengthOfLineEndingAtIndex(input, input_length, i);
      if (length_of_line_ending) {
        out.Append("\r\n", 2);
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
        out.Append(kRFC2047EncodingSuffix, kRFC2047EncodingSuffixLength);
        out.Append("\r\n", 2);
        out.push_back(' ');
      } else {
        out.push_back('=');
        out.Append("\r\n", 2);
      }
      current_line_length = 0;
      if (is_header)
        out.Append(kRFC2047EncodingPrefix, kRFC2047EncodingPrefixLength);
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
    out.Append(kRFC2047EncodingSuffix, kRFC2047EncodingSuffixLength);
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
  QuotedPrintableEncode(utf8_text.c_str(), utf8_text.length(),
                        true /* is_header */, encoded_text);
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

  // |data| may be null if archive file is empty.
  if (!data || data->IsEmpty()) {
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
  if (resources.IsEmpty()) {
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
  if (SchemeRegistry::ShouldTreatURLSchemeAsLocal(url.Protocol()))
    return true;
  if (url.ProtocolIsInHTTPFamily())
    return true;
#if defined(OS_ANDROID)
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
  DCHECK(!boundary.IsEmpty());
  DCHECK(!mime_type.IsEmpty());

  String date_string = MakeRFC2822DateString(date, 0);

  StringBuilder string_builder;
  string_builder.Append("From: <Saved by Blink>\r\n");

  // Add the document URL in the MHTML headers in order to avoid complicated
  // parsing to locate it in the multipart body headers.
  string_builder.Append("Snapshot-Content-Location: ");
  string_builder.Append(url.GetString());

  string_builder.Append("\r\nSubject: ");
  string_builder.Append(ConvertToPrintableCharacters(title));
  string_builder.Append("\r\nDate: ");
  string_builder.Append(date_string);
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

  output_buffer.Append(utf8_string.c_str(), utf8_string.length());
}

void MHTMLArchive::GenerateMHTMLPart(const String& boundary,
                                     const String& content_id,
                                     EncodingPolicy encoding_policy,
                                     const SerializedResource& resource,
                                     Vector<char>& output_buffer) {
  DCHECK(!boundary.IsEmpty());
  DCHECK(content_id.IsEmpty() || content_id[0] == '<');

  StringBuilder string_builder;
  // Per the spec, the boundary must occur at the beginning of a line.
  string_builder.Append("\r\n--");
  string_builder.Append(boundary);
  string_builder.Append("\r\n");

  string_builder.Append("Content-Type: ");
  string_builder.Append(resource.mime_type);
  string_builder.Append("\r\n");

  if (!content_id.IsEmpty()) {
    string_builder.Append("Content-ID: ");
    string_builder.Append(content_id);
    string_builder.Append("\r\n");
  }

  const char* content_encoding = nullptr;
  if (encoding_policy == kUseBinaryEncoding)
    content_encoding = kBinary;
  else if (MIMETypeRegistry::IsSupportedJavaScriptMIMEType(
               resource.mime_type) ||
           MIMETypeRegistry::IsSupportedNonImageMIMEType(resource.mime_type))
    content_encoding = kQuotedPrintable;
  else
    content_encoding = kBase64;

  string_builder.Append("Content-Transfer-Encoding: ");
  string_builder.Append(content_encoding);
  string_builder.Append("\r\n");

  if (!resource.url.ProtocolIsAbout()) {
    string_builder.Append("Content-Location: ");
    string_builder.Append(resource.url.GetString());
    string_builder.Append("\r\n");
  }

  string_builder.Append("\r\n");

  std::string utf8_string = string_builder.ToString().Utf8();
  output_buffer.Append(utf8_string.data(), utf8_string.length());

  if (!strcmp(content_encoding, kBinary)) {
    for (const auto& span : *resource.data)
      output_buffer.Append(span.data(), SafeCast<wtf_size_t>(span.size()));
  } else {
    // FIXME: ideally we would encode the content as a stream without having to
    // fetch it all.
    const SharedBuffer::DeprecatedFlatData flat_data(resource.data);
    const char* data = flat_data.Data();
    wtf_size_t data_length = SafeCast<wtf_size_t>(flat_data.size());
    Vector<char> encoded_data;
    if (!strcmp(content_encoding, kQuotedPrintable)) {
      QuotedPrintableEncode(data, data_length, false /* is_header */,
                            encoded_data);
      output_buffer.Append(encoded_data.data(), encoded_data.size());
    } else {
      DCHECK(!strcmp(content_encoding, kBase64));
      // We are not specifying insertLFs = true below as it would cut the lines
      // with LFs and MHTML requires CRLFs.
      Base64Encode(base::as_bytes(base::make_span(data, data_length)),
                   encoded_data);
      wtf_size_t index = 0;
      wtf_size_t encoded_data_length = encoded_data.size();
      do {
        wtf_size_t line_length =
            std::min(encoded_data_length - index, kMaximumLineLength);
        output_buffer.Append(encoded_data.data() + index, line_length);
        output_buffer.Append("\r\n", 2u);
        index += kMaximumLineLength;
      } while (index < encoded_data_length);
    }
  }
}

void MHTMLArchive::GenerateMHTMLFooterForTesting(const String& boundary,
                                                 Vector<char>& output_buffer) {
  DCHECK(!boundary.IsEmpty());
  std::string utf8_string = String("\r\n--" + boundary + "--\r\n").Utf8();
  output_buffer.Append(utf8_string.c_str(), utf8_string.length());
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
  return subresources_.at(url.GetString());
}

void MHTMLArchive::Trace(blink::Visitor* visitor) {
  visitor->Trace(main_resource_);
  visitor->Trace(subresources_);
}

}  // namespace blink
