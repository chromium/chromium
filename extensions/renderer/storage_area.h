// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_STORAGE_AREA_H_
#define EXTENSIONS_RENDERER_STORAGE_AREA_H_

#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventHandler;
class APIRequestHandler;
class APITypeReferenceMap;
class BindingAccessChecker;

// Implementation of the storage.StorageArea custom type used in the
// chrome.storage API.
class StorageArea {
 public:
  StorageArea(APIRequestHandler* request_handler,
              APIEventHandler* event_handler,
              const APITypeReferenceMap* type_refs,
              const std::string& name,
              const BindingAccessChecker* access_checker);
  ~StorageArea();

  // Creates a StorageArea object for the given context and property name.
  static v8::Local<v8::Object> CreateStorageArea(
      v8::Isolate* isolate,
      const std::string& property_name,
      const base::ListValue* property_values,
      APIRequestHandler* request_handler,
      APIEventHandler* event_handler,
      APITypeReferenceMap* type_refs,
      const BindingAccessChecker* access_checker);

  void HandleFunctionCall(const std::string& method_name,
                          gin::Arguments* arguments);

  v8::Local<v8::Value> GetOnChangedEvent(v8::Isolate* isolate,
                                         v8::Local<v8::Context> context,
                                         v8::Local<v8::Object> wrapper);

 private:
  APIRequestHandler* request_handler_;

  APIEventHandler* event_handler_;

  const APITypeReferenceMap* type_refs_;

  std::string name_;

  const BindingAccessChecker* const access_checker_;

  DISALLOW_COPY_AND_ASSIGN(StorageArea);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_STORAGE_AREA_H_
