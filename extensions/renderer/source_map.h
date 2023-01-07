// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_SOURCE_MAP_H_
#define EXTENSIONS_RENDERER_SOURCE_MAP_H_

#include <string>

#include "v8/include/v8-forward.h"

namespace extensions {

// A map storing resources associated with a given API or utility.
class SourceMap {
 public:
  virtual ~SourceMap() {}

  // Gets the source for the given resource name.
  virtual v8::Local<v8::String> GetSource(v8::Isolate* isolate,
                                          const std::string& name) const = 0;

  // Returns true if the map contains an entry for the given |name|.
  virtual bool Contains(const std::string& name) const = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_SOURCE_MAP_H_
