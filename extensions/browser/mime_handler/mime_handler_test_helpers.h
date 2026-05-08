// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_TEST_HELPERS_H_
#define EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_TEST_HELPERS_H_

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "mojo/public/cpp/system/data_pipe_drainer.h"

namespace extensions {
class StreamContainer;
}  // namespace extensions

namespace extensions::mime_handler {

// Generates a sample `extensions::StreamContainer` for unit tests.
// `container_number` is used as the tab ID and appended to the extension ID
// and all URLs.
std::unique_ptr<extensions::StreamContainer> GenerateSampleStreamContainer(
    int container_number);

// `mojo::DataPipeDrainer::Client` that accumulates received bytes into a
// string and flips a flag on EOF. Drives consumer reads in tests via
// `base::test::RunUntil([&]{ return client.complete(); })`.
//
// `mojo::BlockingCopyToString()` cannot be used against producers that
// close the consumer end via a posted task on the calling sequence
// (e.g., `mojo::DataPipeProducer`, which `MimeHandlerBodyCache::CreatePipe()`
// drives the replay through): its `mojo::Wait` parks the calling
// thread and starves the message loop that owns the close.
class StringDrainerClient : public mojo::DataPipeDrainer::Client {
 public:
  // mojo::DataPipeDrainer::Client:
  void OnDataAvailable(base::span<const uint8_t> data) override;
  void OnDataComplete() override;

  bool complete() const { return complete_; }
  std::string TakeAccumulated() { return std::move(accumulated_); }

 private:
  std::string accumulated_;
  bool complete_ = false;
};

}  // namespace extensions::mime_handler

#endif  // EXTENSIONS_BROWSER_MIME_HANDLER_MIME_HANDLER_TEST_HELPERS_H_
