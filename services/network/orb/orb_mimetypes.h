// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ORB_ORB_MIMETYPES_H_
#define SERVICES_NETWORK_ORB_ORB_MIMETYPES_H_

#include <string_view>

#include "base/component_export.h"

namespace network::orb {

// This enum describes how ORB will classify MIME types (content types).
//
// Note that these values are used in histograms, and must not change.
enum class MimeType {
  // Blocked if served with `X-Content-Type-Options: nosniff` or if this is a
  // 206 range response or if sniffing confirms that the body matches
  // `Content-Type`.
  kHtml = 0,
  kXml = 1,
  kJson = 2,

  // Blocked if served with `X-Content-Type-Options: nosniff` or
  // sniffing detects that this is HTML, JSON or XML.  For example, this
  // behavior is used for `Content-Type: text/plain`.
  kPlain = 3,

  // Blocked if sniffing finds a JSON security prefix.  Used for an otherwise
  // unrecognized type (i.e. type that isn't explicitly recognized as
  // belonging to one of the other categories).
  kOthers = 4,

  // Always blocked.  Used for content types that are unlikely to be
  // incorrectly applied to images, scripts and other legacy no-cors
  // resources.  For example, `Content-Type: application/zip` is blocked
  // without any confirmation sniffing.
  kNeverSniffed = 5,

  kInvalidMimeType,              // For DCHECKs.
  kMaxValue = kInvalidMimeType,  // For UMA histograms.
};

// Returns whether `mime_type` is a Javascript MIME type based on
// https://mimesniff.spec.whatwg.org/#javascript-mime-type
COMPONENT_EXPORT(NETWORK_SERVICE)
bool IsJavascriptMimeType(std::string_view mime_type);

// Returns the representative mime type enum value of the mime type of
// response. For example, this returns the same value for all text/xml mime
// type families such as application/xml, application/rss+xml.
COMPONENT_EXPORT(NETWORK_SERVICE)
MimeType GetCanonicalMimeType(std::string_view mime_type);
}  // namespace network::orb

#endif  // SERVICES_NETWORK_ORB_ORB_MIMETYPES_H_
