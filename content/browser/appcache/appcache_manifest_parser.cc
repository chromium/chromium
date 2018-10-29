// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This is a port of ManifestParser.cc from WebKit/WebCore/loader/appcache.

/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "content/browser/appcache/appcache_manifest_parser.h"

#include <stddef.h>

#include <tuple>
#include <utility>

#include "base/logging.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "url/gurl.h"

namespace content {

namespace {

// Values for the mode in the AppCache manifest parsing algorithm specification.
enum class Mode {
  kExplicit,        // In the CACHE: section.
  kIntercept,       // In the CHROMIUM-INTERCEPT: section. (non-standard)
  kFallback,        // In the FALLBACK: section.
  kOnlineSafelist,  // In the NETWORK: section.
  kUnknown,         // Sections that are not covered by the spec.
};

// AppCache defines whitespace as CR / LF / space (0x20) / tab (0x09).
constexpr bool IsWhiteSpace(char character) {
  return (character == ' ') || (character == '\t') || (character == '\n') ||
         (character == '\r');
}

// AppCache defines newline characters as CR or LF.
constexpr bool IsNewLine(char character) {
  return (character == '\n') || (character == '\r');
}

// AppCache defines token separators as space (0x20) or tab (0x09).
constexpr bool IsTokenSeparator(char character) {
  return (character == ' ') || (character == '\t');
}

// Removes the characters at the beginning of the string up to a newline.
base::StringPiece TrimToFirstNewLine(base::StringPiece data) {
  size_t skip = 0;
  while (skip < data.length() && !IsNewLine(data[skip]))
    ++skip;
  return data.substr(skip);
}

// Removes whitespace characters at the beginning of the string.
base::StringPiece TrimStartingWhiteSpace(base::StringPiece data) {
  size_t skip = 0;
  while (skip < data.length() && IsWhiteSpace(data[skip]))
    ++skip;
  return data.substr(skip);
}

// Removes whitespace characters at the end of the string.
base::StringPiece TrimTrailingWhiteSpace(base::StringPiece data) {
  size_t length = data.size();

  while (length != 0) {
    --length;
    if (!IsWhiteSpace(data[length])) {
      ++length;
      break;
    }
  }
  return data.substr(0, length);
}

// Splits a string at the first occurrence of a newline.
//
// Returns the first line, which is guaranteed not to include a newline, and the
// rest of the string, which may be empty.
std::pair<base::StringPiece, base::StringPiece> SplitOnNewLine(
    base::StringPiece data) {
  size_t split = 0;
  while (split < data.length() && !IsNewLine(data[split]))
    ++split;
  return {data.substr(0, split), data.substr(split)};
}

// True if the string does not contain any newline character.
bool IsSingleLine(base::StringPiece maybe_line) {
  return !std::any_of(maybe_line.begin(), maybe_line.end(), &IsNewLine);
}

// Splits a token out of a manifest line.
//
// Tokens are separated by space (0x20) or tab (0x09) characters.
//
// The line must not start with a whitespace character.
//
// Returns the token and the rest of the line. Consumes the whitespace after the
// returned token -- the rest of the line will not start with whitespace.
std::pair<base::StringPiece, base::StringPiece> SplitLineToken(
    base::StringPiece line) {
  DCHECK(IsSingleLine(line));
  DCHECK(line.empty() || !IsWhiteSpace(line[0]));

  size_t token_end = 0;
  while (token_end < line.length() && !IsTokenSeparator(line[token_end]))
    ++token_end;

  size_t split = token_end;
  while (split < line.length() && IsTokenSeparator(line[split]))
    ++split;

  return {line.substr(0, token_end), line.substr(split)};
}

// True if the given line is a mode-setting line.
//
// In the AppCache parsing algorithm, the mode only changes when processing a
// line that ends with ':' (colon) after whitespace removal.
//
// The given string must have had whitespace stripped at both ends.
bool IsModeSettingLine(base::StringPiece line) {
  DCHECK(IsSingleLine(line));

  if (line.empty())
    return false;

  DCHECK(!IsWhiteSpace(line[0])) << "line starts with whitespace";

  const auto last_character = line[line.length() - 1];
  DCHECK(!IsWhiteSpace(last_character)) << "line ends with whitespace";

  return last_character == ':';
}

// The mode that the AppCache parsing algorithm will be switched to.
//
// The given string must be a mode-setting line.
Mode ParseModeSettingLine(base::StringPiece line) {
  DCHECK(IsModeSettingLine(line));

  static constexpr base::StringPiece kCacheLine("CACHE:");
  if (line == kCacheLine)
    return Mode::kExplicit;

  static constexpr base::StringPiece kFallbackLine("FALLBACK:");
  if (line == kFallbackLine)
    return Mode::kFallback;

  static constexpr base::StringPiece kNetworkLine("NETWORK:");
  if (line == kNetworkLine)
    return Mode::kOnlineSafelist;

  static constexpr base::StringPiece kInterceptLine("CHROMIUM-INTERCEPT:");
  if (line == kInterceptLine)
    return Mode::kIntercept;

  return Mode::kUnknown;
}

// True if the next token in the manifest line is the pattern indicator flag.
//
// Pattern URLs are a non-standard feature.
bool NextTokenIsPatternMatchingFlag(base::StringPiece line) {
  base::StringPiece is_pattern_token;
  std::tie(is_pattern_token, line) = SplitLineToken(line);

  static constexpr base::StringPiece kPatternFlag("isPattern");
  return is_pattern_token == kPatternFlag;
}

// Parses a URL token in an AppCache manifest.
//
// The returned URL may not be valid, if the token does not represent a valid
// URL.
//
// Per the AppCache specification, the URL is resolved relative to the manifest
// URL, and stripped of any fragment.
GURL ParseUrlToken(base::StringPiece url_token, const GURL& manifest_url) {
  GURL url = manifest_url.Resolve(url_token);
  if (!url.is_valid())
    return url;

  if (url.has_ref()) {
    GURL::Replacements replacements;
    replacements.ClearRef();
    url = url.ReplaceComponents(replacements);
  }
  return url;
}

bool ScopeMatches(const GURL& manifest_url, const GURL& namespace_url) {
  return base::StartsWith(namespace_url.spec(),
                          manifest_url.GetWithoutFilename().spec(),
                          base::CompareCase::SENSITIVE);
}

}  // namespace

AppCacheManifest::AppCacheManifest() = default;

AppCacheManifest::~AppCacheManifest() = default;

bool ParseManifest(const GURL& manifest_url,
                   const char* manifest_bytes,
                   int manifest_size,
                   ParseMode parse_mode,
                   AppCacheManifest& manifest) {
  // The parsing algorithm is specified at
  //   https://html.spec.whatwg.org/multipage/offline.html

  DCHECK(manifest.explicit_urls.empty());
  DCHECK(manifest.fallback_namespaces.empty());
  DCHECK(manifest.online_whitelist_namespaces.empty());
  DCHECK(!manifest.online_whitelist_all);
  DCHECK(!manifest.did_ignore_intercept_namespaces);
  DCHECK(!manifest.did_ignore_fallback_namespaces);

  Mode mode = Mode::kExplicit;

  // The specification requires UTF-8-decoding the manifest, which replaces
  // invalid UTF-8 characters with placeholders. It would be nice if
  // utf_string_conversions included a UTF-8 to UTF-8 conversion for this
  // purpose, but AppCache isn't important enough to add conversion code just
  // to accelerate manifest decoding.
  DCHECK_GE(manifest_size, 0);
  base::string16 wide_manifest_bytes =
      base::UTF8ToUTF16(base::StringPiece(manifest_bytes, manifest_size));
  std::string decoded_manifest_bytes = base::UTF16ToUTF8(wide_manifest_bytes);

  // The bytes of the manifest that haven't been consumed yet.
  base::StringPiece data(decoded_manifest_bytes);

  // Discard a leading UTF-8 Byte-Order-Mark (BOM) (0xEF, 0xBB, 0xBF);
  static constexpr base::StringPiece kUtf8Bom("\xEF\xBB\xBF");
  if (data.starts_with(kUtf8Bom))
    data = data.substr(kUtf8Bom.length());

  // The manifest has to start with a well-defined signature.
  static constexpr base::StringPiece kSignature("CACHE MANIFEST");
  static constexpr base::StringPiece kChromiumSignature(
      "CHROMIUM CACHE MANIFEST");
  if (data.starts_with(kSignature)) {
    data = data.substr(kSignature.length());
  } else if (data.starts_with(kChromiumSignature)) {
    // Chrome recognizes a separate signature, CHROMIUM CACHE MANIFEST. This was
    // built so that manifests that use the Chrome-only feature
    // CHROMIUM-INTERCEPT will be ignored by other browsers.
    // See https://crbug.com/101565

    // TODO(pwnall): Add a UMA metric to see if we can remove support for this
    //               non-standard signature.
    data = data.substr(kChromiumSignature.length());
  } else {
    return false;
  }

  // The character after "CACHE MANIFEST" must be a whitespace character.
  if (!data.empty() && !IsWhiteSpace(data[0]))
    return false;

  // The spec requires ignoring any characters on the first line after the
  // signature and its following whitespace.
  data = TrimToFirstNewLine(data);

  while (true) {
    data = TrimStartingWhiteSpace(data);
    if (data.empty())
      break;

    base::StringPiece line;
    std::tie(line, data) = SplitOnNewLine(data);

    // The checks above guarantee that the input to SplitOnNewLine() starts with
    // a non-whitespace character.
    DCHECK(!line.empty());

    if (line[0] == '#')  // Lines starting with # are comments.
      continue;

    line = TrimTrailingWhiteSpace(line);

    // Handle all the steps checking for lines that end with ":".
    if (IsModeSettingLine(line)) {
      mode = ParseModeSettingLine(line);
      continue;
    }

    if (mode == Mode::kUnknown)
      continue;

    static constexpr base::StringPiece kOnlineSafelistWildcard("*");
    if (mode == Mode::kOnlineSafelist && line == kOnlineSafelistWildcard) {
      manifest.online_whitelist_all = true;
      continue;
    }

    // Chrome does not implement the SETTINGS: section. If we ever decided to do
    // so, the implementation would go here.

    // Common code for the following sections: explicit (CACHE:),
    // fallback (FALLBACK:), online safelist (NETWORK:) and intercept
    // (CHROMIUM-INTERCEPT:). All these sections start by parsing a URL token.
    base::StringPiece namespace_url_token;
    std::tie(namespace_url_token, line) = SplitLineToken(line);
    GURL namespace_url = ParseUrlToken(namespace_url_token, manifest_url);
    if (!namespace_url.is_valid())
      continue;

    if (mode == Mode::kExplicit || mode == Mode::kOnlineSafelist) {
      // Scheme component must be the same as the manifest URL's.
      if (namespace_url.scheme() != manifest_url.scheme()) {
        continue;
      }

      // Deviate from the HTML5 spec by supporting the caching of cross-origin
      // HTTPS resources. See https://crbug.com/69594
      //
      // Per the spec, explicit (CACHE:) cross-origin HTTPS resources should be
      // ignored here. We've opted for a milder constraint and allow caching
      // unless the resource has a "no-store" header. That condition is enforced
      // in AppCacheUpdateJob.

      if (mode == Mode::kExplicit) {
        manifest.explicit_urls.insert(namespace_url.spec());
      } else {
        // Chrome supports URL patterns in manifests. This is not standardized.
        // An URL record followed by the "isPattern" token is considered a
        // pattern.

        // TODO(pwnall): Add a UMA metric to see if we can remove this feature.
        bool is_pattern = NextTokenIsPatternMatchingFlag(line);
        manifest.online_whitelist_namespaces.emplace_back(AppCacheNamespace(
            APPCACHE_NETWORK_NAMESPACE, namespace_url, GURL(), is_pattern));
      }
      continue;
    }

    if (mode == Mode::kIntercept) {
      // Chrome supports a CHROMIUM-INTERCEPT section.  https://crbug.com/101565
      //
      // This section consists of entries of the form:
      // namespace_url verb url_target

      if (parse_mode != PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES) {
        manifest.did_ignore_intercept_namespaces = true;
        continue;
      }

      if (manifest_url.GetOrigin() != namespace_url.GetOrigin())
        continue;

      // The only supported verb is "return".
      base::StringPiece verb_token;
      std::tie(verb_token, line) = SplitLineToken(line);
      static constexpr base::StringPiece kReturnVerb("return");
      if (verb_token != kReturnVerb)
        continue;

      base::StringPiece target_url_token;
      std::tie(target_url_token, line) = SplitLineToken(line);
      if (target_url_token.empty())
        continue;
      GURL target_url = ParseUrlToken(target_url_token, manifest_url);
      if (!target_url.is_valid())
        continue;

      if (manifest_url.GetOrigin() != target_url.GetOrigin())
        continue;

      // TODO(pwnall): Add a UMA metric to see if we can remove this feature.
      bool is_pattern = NextTokenIsPatternMatchingFlag(line);
      manifest.intercept_namespaces.emplace_back(
          APPCACHE_INTERCEPT_NAMESPACE, namespace_url, target_url, is_pattern);
      continue;
    }

    if (mode == Mode::kFallback) {
      if (manifest_url.GetOrigin() != namespace_url.GetOrigin())
        continue;

      if (parse_mode != PARSE_MANIFEST_ALLOWING_DANGEROUS_FEATURES) {
        if (!ScopeMatches(manifest_url, namespace_url)) {
          manifest.did_ignore_fallback_namespaces = true;
          continue;
        }
      }

      base::StringPiece fallback_url_token;
      std::tie(fallback_url_token, line) = SplitLineToken(line);
      if (fallback_url_token.empty())
        continue;
      GURL fallback_url = ParseUrlToken(fallback_url_token, manifest_url);
      if (!fallback_url.is_valid())
        continue;

      if (manifest_url.GetOrigin() != fallback_url.GetOrigin())
        continue;

      // TODO(pwnall): Add a UMA metric to see if we can remove this feature.
      bool is_pattern = NextTokenIsPatternMatchingFlag(line);

      // Store regardless of duplicate namespace URL. Only the first match will
      // ever be used.
      manifest.fallback_namespaces.emplace_back(
          AppCacheNamespace(APPCACHE_FALLBACK_NAMESPACE, namespace_url,
                            fallback_url, is_pattern));
      continue;
    }

    NOTREACHED() << "Unimplemented AppCache manifest parser mode";
  }

  return true;
}

}  // namespace content
