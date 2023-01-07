// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_NATIVE_HANDLER_H_
#define EXTENSIONS_RENDERER_NATIVE_HANDLER_H_

#include "v8/include/v8-forward.h"

namespace extensions {

// NativeHandlers are intended to be used with a ModuleSystem. The ModuleSystem
// will assume ownership of the NativeHandler, and as a ModuleSystem is tied to
// a single v8::Context, this implies that NativeHandlers will also be tied to
// a single v8::Context.
// TODO(koz): Rename this to NativeJavaScriptModule.
class NativeHandler {
 public:
  NativeHandler();

  NativeHandler(const NativeHandler&) = delete;
  NativeHandler& operator=(const NativeHandler&) = delete;

  virtual ~NativeHandler();

  // Initializes the native handler.
  virtual void Initialize() = 0;

  // Returns true if the handler has been initialized.
  virtual bool IsInitialized() = 0;

  // Create a new instance of the object this handler specifies.
  virtual v8::Local<v8::Object> NewInstance() = 0;

  // Invalidate this object so it cannot be used any more. This is needed
  // because it's possible for this to outlive its owner context. Invalidate
  // must be called before this happens.
  //
  // Subclasses should override to invalidate their own V8 state. If they do
  // they must call their superclass' Invalidate().
  //
  // Invalidate() will be called on destruction, if it hasn't already been.
  // Subclasses don't need to do it themselves.
  virtual void Invalidate();

 protected:
  // Allow subclasses to query valid state.
  bool is_valid() { return is_valid_; }

 private:
  bool is_valid_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_NATIVE_HANDLER_H_
