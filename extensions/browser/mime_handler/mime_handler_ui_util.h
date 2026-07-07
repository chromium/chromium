// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_UI_UTIL_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_UI_UTIL_H_

namespace content {
class WebContents;
}

namespace extensions {
class Extension;

namespace mime_handler {

// Returns the extension rendering the primary main frame of
// `web_contents` as a top-level generic MIME handler, or nullptr if no
// such extension is active. Returns nullptr for embedded (`<embed>` /
// `<iframe>`) handlers, allowlisted plugin extensions
// (`MimeTypesHandler::IsPluginExtension`, which covers built-in PDF),
// and extensions that have been uninstalled or disabled since the
// stream was dispatched.
const Extension* GetTopLevelMimeHandlerExtension(
    content::WebContents& web_contents);

}  // namespace mime_handler
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_UI_UTIL_H_
