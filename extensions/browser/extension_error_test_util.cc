// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/extension_error_test_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "content/public/common/url_constants.h"
#include "extensions/browser/extension_error.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/stack_frame.h"
#include "url/gurl.h"

namespace extensions {
namespace error_test_util {

namespace {
const char16_t kDefaultStackTrace[] = u"function_name (https://url.com:1:1)";
}

std::unique_ptr<ExtensionError> CreateNewRuntimeError(
    const ExtensionId& extension_id,
    const std::string& message,
    bool from_incognito) {
  StackTrace stack_trace;
  std::unique_ptr<StackFrame> frame =
      StackFrame::CreateFromText(kDefaultStackTrace);
  CHECK(frame.get());
  stack_trace.push_back(*frame);

  std::u16string source =
      base::UTF8ToUTF16(std::string(kExtensionScheme) +
                        url::kStandardSchemeSeparator + extension_id);

  return std::unique_ptr<ExtensionError>(
      new RuntimeError(extension_id, from_incognito, source,
                       base::UTF8ToUTF16(message), stack_trace,
                       GURL(),  // no context url
                       logging::LOGGING_ERROR,
                       0,    // Render frame id
                       0));  // Render process id
}

std::unique_ptr<ExtensionError> CreateNewRuntimeError(
    const ExtensionId& extension_id,
    const std::string& message) {
  return CreateNewRuntimeError(extension_id, message, false);
}

std::unique_ptr<ExtensionError> CreateNewManifestError(
    const ExtensionId& extension_id,
    const std::string& message) {
  return std::unique_ptr<ExtensionError>(
      new ManifestError(extension_id, base::UTF8ToUTF16(message), std::string(),
                        std::u16string()));
}

}  // namespace error_test_util
}  // namespace extensions
