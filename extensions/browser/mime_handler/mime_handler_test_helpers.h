// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_TEST_HELPERS_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_TEST_HELPERS_H_

#include <memory>

namespace extensions {
class StreamContainer;
}  // namespace extensions

namespace extensions::mime_handler {

// Generates a sample `extensions::StreamContainer` for unit tests.
// `container_number` is used as the tab ID and appended to the extension ID
// and all URLs.
std::unique_ptr<extensions::StreamContainer> GenerateSampleStreamContainer(
    int container_number);

}  // namespace extensions::mime_handler

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_TEST_HELPERS_H_
