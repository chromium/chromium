// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SAFE_BUILTINS_H_
#define EXTENSIONS_RENDERER_SAFE_BUILTINS_H_

#include <memory>

#include "base/macros.h"
#include "v8/include/v8.h"

namespace extensions {
class ScriptContext;

// A collection of safe builtin objects, in that they won't be tainted by
// extensions overriding methods on them.
class SafeBuiltins {
 public:
  // Creates the v8::Extension which manages SafeBuiltins instances.
  static std::unique_ptr<v8::Extension> CreateV8Extension();

  explicit SafeBuiltins(ScriptContext* context);
  ~SafeBuiltins();

  // Each method returns an object with methods taken from their respective
  // builtin object's prototype, adapted to automatically call() themselves.
  //
  // Examples:
  //   Array.prototype.forEach.call(...) becomes Array.forEach(...)
  //   Object.prototype.toString.call(...) becomes Object.toString(...)
  //   Object.keys.call(...) becomes Object.keys(...)
  v8::Local<v8::Object> GetArray() const;
  v8::Local<v8::Object> GetFunction() const;
  v8::Local<v8::Object> GetJSON() const;
  // NOTE(kalman): VS2010 won't compile "GetObject", it mysteriously renames it
  // to "GetObjectW" - hence GetObjekt. Sorry.
  v8::Local<v8::Object> GetObjekt() const;
  v8::Local<v8::Object> GetRegExp() const;
  v8::Local<v8::Object> GetString() const;
  v8::Local<v8::Object> GetError() const;
  v8::Local<v8::Object> GetPromise() const;

 private:
  ScriptContext* context_;

  DISALLOW_COPY_AND_ASSIGN(SafeBuiltins);
};

}  //  namespace extensions

#endif  // EXTENSIONS_RENDERER_SAFE_BUILTINS_H_
