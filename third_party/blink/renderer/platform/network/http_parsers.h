/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_HTTP_PARSERS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_NETWORK_HTTP_PARSERS_H_

#include "third_party/blink/renderer/platform/network/parsed_content_type.h"
#include "third_party/blink/renderer/platform/network/server_timing_header.h"
#include "third_party/blink/renderer/platform/platform_export.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/hash_set.h"
#include "third_party/blink/renderer/platform/wtf/text/string_hash.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <stdint.h>
#include <memory>

namespace blink {

class HTTPHeaderMap;
class ResourceResponse;

enum ContentTypeOptionsDisposition {
  kContentTypeOptionsNone,
  kContentTypeOptionsNosniff
};

// Be aware that some behavior may depend on this enum's ordering, with
// higher values taking precedence over lower ones.
enum ReflectedXSSDisposition {
  kReflectedXSSUnset = 0,
  kAllowReflectedXSS,
  kReflectedXSSInvalid,
  kFilterReflectedXSS,
  kBlockReflectedXSS
};

using CommaDelimitedHeaderSet = HashSet<String, CaseFoldingHash>;

struct CacheControlHeader {
  DISALLOW_NEW();
  bool parsed : 1;
  bool contains_no_cache : 1;
  bool contains_no_store : 1;
  bool contains_must_revalidate : 1;
  double max_age;
  double stale_while_revalidate;

  CacheControlHeader()
      : parsed(false),
        contains_no_cache(false),
        contains_no_store(false),
        contains_must_revalidate(false),
        max_age(0.0),
        stale_while_revalidate(0.0) {}
};

using ServerTimingHeaderVector = Vector<std::unique_ptr<ServerTimingHeader>>;

PLATFORM_EXPORT bool IsContentDispositionAttachment(const String&);
PLATFORM_EXPORT bool IsValidHTTPHeaderValue(const String&);
// Checks whether the given string conforms to the |token| ABNF production
// defined in the RFC 7230 or not.
//
// The ABNF is for validating octets, but this method takes a String instance
// for convenience which consists of Unicode code points. When this method sees
// non-ASCII characters, it just returns false.
PLATFORM_EXPORT bool IsValidHTTPToken(const String&);
// |matcher| specifies a function to check a whitespace character. if |nullptr|
// is specified, ' ' and '\t' are treated as whitespace characters.
PLATFORM_EXPORT bool ParseHTTPRefresh(const String& refresh,
                                      WTF::CharacterMatchFunctionPtr matcher,
                                      double& delay,
                                      String& url);
PLATFORM_EXPORT double ParseDate(const String&);

// Given a Media Type (like "foo/bar; baz=gazonk" - usually from the
// 'Content-Type' HTTP header), extract and return the "type/subtype" portion
// ("foo/bar").
//
// Note:
// - This function does not in any way check that the "type/subtype" pair
//   is well-formed.
// - OWSes at the head and the tail of the region before the first semicolon
//   are trimmed.
PLATFORM_EXPORT AtomicString ExtractMIMETypeFromMediaType(const AtomicString&);

// Given an X-XSS-Protection value like "1; mode=block; report=/foo", combine
// the first positional parameter and the "mode" into the result code, and
// return the "report" as report_url, if present. Return kReflectedXSSInvalid
// on bad syntax, setting |failure_reason| and |failure_position|, otherwise
// set |failure_position| to the start of the "report" URL, if present (since
// it is not validated here, and the caller may need that position information
// to construct an error message).
PLATFORM_EXPORT ReflectedXSSDisposition
ParseXSSProtectionHeader(const String& header,
                         String& failure_reason,
                         unsigned& failure_position,
                         String& report_url);

PLATFORM_EXPORT CacheControlHeader
ParseCacheControlDirectives(const AtomicString& cache_control_header,
                            const AtomicString& pragma_header);
PLATFORM_EXPORT void ParseCommaDelimitedHeader(const String& header_value,
                                               CommaDelimitedHeaderSet&);

PLATFORM_EXPORT ContentTypeOptionsDisposition
ParseContentTypeOptionsHeader(const String& header);

// Returns true and stores the position of the end of the headers to |*end|
// if the headers part ends in |bytes[0..size]|. Returns false otherwise.
PLATFORM_EXPORT bool ParseMultipartFormHeadersFromBody(
    const char* bytes,
    wtf_size_t,
    HTTPHeaderMap* header_fields,
    wtf_size_t* end);

// Returns true and stores the position of the end of the headers to |*end|
// if the headers part ends in |bytes[0..size]|. Returns false otherwise.
PLATFORM_EXPORT bool ParseMultipartHeadersFromBody(const char* bytes,
                                                   wtf_size_t,
                                                   ResourceResponse*,
                                                   wtf_size_t* end);

// Extracts the values in a Content-Range header and returns true if all three
// values are present and valid for a 206 response; otherwise returns false.
// The following values will be outputted:
// |*first_byte_position| = inclusive position of the first byte of the range
// |*last_byte_position| = inclusive position of the last byte of the range
// |*instance_length| = size in bytes of the object requested
// If this method returns false, then all of the outputs will be -1.
PLATFORM_EXPORT bool ParseContentRangeHeaderFor206(const String& content_range,
                                                   int64_t* first_byte_position,
                                                   int64_t* last_byte_position,
                                                   int64_t* instance_length);

PLATFORM_EXPORT std::unique_ptr<ServerTimingHeaderVector>
ParseServerTimingHeader(const String&);

using Mode = blink::ParsedContentType::Mode;

}  // namespace blink

#endif
