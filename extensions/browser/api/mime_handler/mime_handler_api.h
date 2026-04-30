// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_MIME_HANDLER_MIME_HANDLER_API_H_
#define EXTENSIONS_BROWSER_API_MIME_HANDLER_MIME_HANDLER_API_H_

#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_histogram_value.h"

namespace extensions {

class MimeHandlerGetStreamInfoFunction : public ExtensionFunction {
 public:
  MimeHandlerGetStreamInfoFunction();
  MimeHandlerGetStreamInfoFunction(const MimeHandlerGetStreamInfoFunction&) =
      delete;
  MimeHandlerGetStreamInfoFunction& operator=(
      const MimeHandlerGetStreamInfoFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mimeHandler.getStreamInfo",
                             MIMEHANDLER_GETSTREAMINFO)

 protected:
  ~MimeHandlerGetStreamInfoFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MIME_HANDLER_MIME_HANDLER_API_H_
