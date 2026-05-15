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

class MimeHandlerAbortAndFallbackToNativeHandlerFunction
    : public ExtensionFunction {
 public:
  MimeHandlerAbortAndFallbackToNativeHandlerFunction();
  MimeHandlerAbortAndFallbackToNativeHandlerFunction(
      const MimeHandlerAbortAndFallbackToNativeHandlerFunction&) = delete;
  MimeHandlerAbortAndFallbackToNativeHandlerFunction& operator=(
      const MimeHandlerAbortAndFallbackToNativeHandlerFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mimeHandler.abortAndFallbackToNativeHandler",
                             MIMEHANDLER_ABORTANDFALLBACKTONATIVEHANDLER)

 protected:
  ~MimeHandlerAbortAndFallbackToNativeHandlerFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MimeHandlerSetMimeHandlerOptionsFunction : public ExtensionFunction {
 public:
  MimeHandlerSetMimeHandlerOptionsFunction();
  MimeHandlerSetMimeHandlerOptionsFunction(
      const MimeHandlerSetMimeHandlerOptionsFunction&) = delete;
  MimeHandlerSetMimeHandlerOptionsFunction& operator=(
      const MimeHandlerSetMimeHandlerOptionsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mimeHandler.setMimeHandlerOptions",
                             MIMEHANDLER_SETMIMEHANDLEROPTIONS)

 protected:
  ~MimeHandlerSetMimeHandlerOptionsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

class MimeHandlerGetMimeHandlerOptionsFunction : public ExtensionFunction {
 public:
  MimeHandlerGetMimeHandlerOptionsFunction();
  MimeHandlerGetMimeHandlerOptionsFunction(
      const MimeHandlerGetMimeHandlerOptionsFunction&) = delete;
  MimeHandlerGetMimeHandlerOptionsFunction& operator=(
      const MimeHandlerGetMimeHandlerOptionsFunction&) = delete;

  DECLARE_EXTENSION_FUNCTION("mimeHandler.getMimeHandlerOptions",
                             MIMEHANDLER_GETMIMEHANDLEROPTIONS)

 protected:
  ~MimeHandlerGetMimeHandlerOptionsFunction() override;

  // ExtensionFunction:
  ResponseAction Run() override;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_MIME_HANDLER_MIME_HANDLER_API_H_
