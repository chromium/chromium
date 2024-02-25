// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_WAKE_EVENT_PAGE_H_
#define EXTENSIONS_RENDERER_WAKE_EVENT_PAGE_H_

#include "v8/include/v8-forward.h"

namespace extensions {
class ScriptContext;

// This class implements the wake-event-page JavaScript function, which wakes
// an event page and runs a callback when done.
//
// Note, the function will do a round trip to the browser even if event page is
// open. Any optimisation to prevent this must be at the JavaScript level.
class WakeEventPage
{
 public:
  // Returns the wake-event-page function bound to a given context. The
  // function will be cached as a hidden value in the context's global object.
  //
  // To mix C++ and JavaScript, example usage might be:
  //
  // WakeEventPage::GetForContext(context)(function() {
  //   ...
  // });
  //
  // Thread safe.
  static v8::Local<v8::Function> GetForContext(ScriptContext* context);

  WakeEventPage() = delete;

 private:
  class WakeEventPageNativeHandler;
};

}  //  namespace extensions

#endif  // EXTENSIONS_RENDERER_WAKE_EVENT_PAGE_H_
