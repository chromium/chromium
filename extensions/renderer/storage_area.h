// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_STORAGE_AREA_H_
#define EXTENSIONS_RENDERER_STORAGE_AREA_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/values.h"
#include "v8/include/v8-forward.h"

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

  StorageArea(const StorageArea&) = delete;
  StorageArea& operator=(const StorageArea&) = delete;

  ~StorageArea();

  // Creates a StorageArea object for the given context and property name.
  static v8::Local<v8::Object> CreateStorageArea(
      v8::Isolate* isolate,
      const std::string& property_name,
      const base::Value::List* property_values,
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
  raw_ptr<APIRequestHandler, DanglingUntriaged> request_handler_;

  raw_ptr<APIEventHandler, DanglingUntriaged> event_handler_;

  raw_ptr<const APITypeReferenceMap, DanglingUntriaged> type_refs_;

  std::string name_;

  const raw_ptr<const BindingAccessChecker, DanglingUntriaged> access_checker_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_STORAGE_AREA_H_
