// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_PUBLIC_CPP_BINDINGS_STRING_DATA_VIEW_H_
#define MOJO_PUBLIC_CPP_BINDINGS_STRING_DATA_VIEW_H_

#include "base/memory/raw_ptr_exclusion.h"
#include "mojo/public/cpp/bindings/lib/array_internal.h"

namespace mojo {

class Message;

// Access to the contents of a serialized string.
class StringDataView {
 public:
  StringDataView() {}

  StringDataView(internal::String_Data* data, Message* message) : data_(data) {}

  bool is_null() const { return !data_; }

  const char* storage() const { return data_->storage(); }

  size_t size() const { return data_->size(); }

 private:
  // RAW_PTR_EXCLUSION: Performance reasons: based on this sampling profiler
  // result on Mac. go/brp-mac-prof-diff-20230403
  RAW_PTR_EXCLUSION internal::String_Data* data_ = nullptr;
};

}  // namespace mojo

#endif  // MOJO_PUBLIC_CPP_BINDINGS_STRING_DATA_VIEW_H_
