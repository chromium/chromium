// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_PAGE_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_PAGE_H_

#include <string>

class GURL;

namespace extensions {

// Returns an HTML template page that embeds a MIME handler for `mime_type`.
// `internal_id` is written into the element's `internalid` attribute so
// MimeHandlerStreamManager can match the stream to the frame once the
// initial about:blank navigation commits.
//
// When `use_oopif` is true, the page contains an iframe (for OOPIF-based
// handlers like PDF and generic MIME handlers).  Otherwise uses the legacy
// GuestView embed template with a background color derived from the plugin info
// registered for `resource_url`.
std::string CreateTemplateMimeHandlerPage(const GURL& resource_url,
                                          const std::string& mime_type,
                                          const std::string& internal_id,
                                          bool use_oopif);

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_PAGE_H_
