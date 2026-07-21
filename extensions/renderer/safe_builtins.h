// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SAFE_BUILTINS_H_
#define EXTENSIONS_RENDERER_SAFE_BUILTINS_H_

#include <memory>
#include <string>
#include <unordered_map>

#include "base/memory/raw_ptr.h"
#include "v8/include/v8-forward.h"
#include "v8/include/v8-persistent-handle.h"

namespace extensions {
class ScriptContext;

// A collection of safe builtin objects, in that they won't be tainted by
// extensions overriding methods on them.
class SafeBuiltins {
 public:
  explicit SafeBuiltins(ScriptContext* context);

  SafeBuiltins(const SafeBuiltins&) = delete;
  SafeBuiltins& operator=(const SafeBuiltins&) = delete;

  ~SafeBuiltins();

  // Clears all persistent handles held by SafeBuiltins.
  void Reset();

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
  v8::Local<v8::Object> GetSymbol() const;

 private:
  struct UntaintedBuiltins;
  struct BuiltinInfo;

  using V8MethodMap = std::unordered_map<std::string, v8::Global<v8::Function>>;

  // Creates a safe version of a builtin object.
  // `ctor` is the original constructor (can be empty).
  // `proto_methods` are methods from the prototype to be copied.
  // `static_methods` are static methods from the constructor to be copied.
  // Returns a new object with these methods configured to call themselves
  // safely.
  v8::Local<v8::Object> CreateSafeBuiltin(
      v8::Local<v8::Function> ctor,
      const V8MethodMap& proto_methods,
      const V8MethodMap& static_methods) const;

  // Helper to lazily create and cache a safe builtin object.
  // `safe_object` is the persistent handle where the cached object is stored.
  // `info` contains the constructor and methods to copy.
  v8::Local<v8::Object> GetOrCreateSafeBuiltin(
      v8::Global<v8::Object>& safe_object,
      const BuiltinInfo& info) const;

  raw_ptr<ScriptContext> context_;
  std::unique_ptr<UntaintedBuiltins> untainted_;

  // Cached safe versions of the builtin objects. Created lazily on first
  // access.
  mutable v8::Global<v8::Object> safe_array_;
  mutable v8::Global<v8::Object> safe_function_;
  mutable v8::Global<v8::Object> safe_json_;
  mutable v8::Global<v8::Object> safe_object_;
  mutable v8::Global<v8::Object> safe_regexp_;
  mutable v8::Global<v8::Object> safe_string_;
  mutable v8::Global<v8::Object> safe_error_;
  mutable v8::Global<v8::Object> safe_promise_;
  mutable v8::Global<v8::Object> safe_symbol_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SAFE_BUILTINS_H_
