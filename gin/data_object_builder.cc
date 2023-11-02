// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/data_object_builder.h"

namespace gin {

DataObjectBuilder::DataObjectBuilder(v8::Isolate* isolate)
    : isolate_(isolate),
      context_(isolate->GetCurrentContext()),
      object_(v8::Object::New(isolate)) {}

DataObjectBuilder::~DataObjectBuilder() = default;

}  // namespace gin
