// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_LOGGER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_LOGGER_H_

#include <string_view>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-persistent-handle.h"

namespace auction_worklet {

class AuctionV8Helper;

// Helper class to log text to the console by using methods from the `console`
// object. Must be destroyed before the associated v8::Context, and created
// before the context has been used to run any scripts, as scripts can modify or
// replace the console object. Uses a v8::Global to persist past the Isolate's
// HandleScope. To ensure it's created at the right time should only call
// immediately after AuctionV8Logger::CreateContext().
//
// Its logging methods are expected to be invoked in C++ methods that are
// invoked from Javascript, while the v8::Context used to create logger is
// active.
class CONTENT_EXPORT AuctionV8Logger {
 public:
  AuctionV8Logger(AuctionV8Helper* v8_helper, v8::Local<v8::Context> context);
  AuctionV8Logger(const AuctionV8Logger&) = delete;
  ~AuctionV8Logger();

  AuctionV8Logger& operator=(const AuctionV8Logger&) = delete;

  // Logs the provided warning message to the console. Expects the v8::Context
  // that was passed to the constructor to be the currently active Context.
  //
  // If `message` cannot be converted to a v8 string, silently drops the
  // message.
  void LogConsoleWarning(std::string_view message);

 private:
  friend class AuctionV8Logger;

  v8::Global<v8::Function> console_warn_;
  const raw_ptr<AuctionV8Helper> v8_helper_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_AUCTION_V8_LOGGER_H_
