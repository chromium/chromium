// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_CONSOLE_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_CONSOLE_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "v8/include/v8-forward.h"

namespace auction_worklet {

class AuctionV8Helper;

// A basic interim implementation of console output. This doesn't support
// format specifiers.  Is owned by an AuctionV8Helper (keeping a back pointer),
// and writes to AuctionV8Helper::console_buffer() active at time of any JS call
// it handles.
class Console {
 public:
  Console(const Console&) = delete;
  Console& operator=(const Console&) = delete;
  ~Console();

  // Returns an object template that provides a minimal replacement for the
  // standard console object. Refers to `this`, so should not outlast it.
  v8::Local<v8::ObjectTemplate> GetConsoleTemplate();

 private:
  friend class AuctionV8Helper;
  typedef void(ConsoleFn)(const v8::FunctionCallbackInfo<v8::Value>&);

  // Should not outlast `v8_helper` (which is the only thing that should create
  // this in the first place).
  explicit Console(AuctionV8Helper* v8_helper);

  void RegisterConsoleMethod(v8::Local<v8::External> v8_this,
                             const char* name,
                             ConsoleFn function,
                             v8::Local<v8::ObjectTemplate> console_template);

  static void ConsoleDebug(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ConsoleError(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ConsoleInfo(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ConsoleLog(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void ConsoleWarn(const v8::FunctionCallbackInfo<v8::Value>& args);

  void DoConsoleOut(const std::string& prefix,
                    const v8::FunctionCallbackInfo<v8::Value>& args);

  const raw_ptr<AuctionV8Helper> v8_helper_;
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_CONSOLE_H_
