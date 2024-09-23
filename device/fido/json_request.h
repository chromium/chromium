#ifndef DEVICE_FIDO_JSON_REQUEST_H_
#define DEVICE_FIDO_JSON_REQUEST_H_

// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/component_export.h"
#include "base/memory/ref_counted.h"

namespace base {
class Value;
}

namespace device {

// JSONRequest wraps `base::Value` in a reference-counted structure. A
// `base::Value` is a move-only type, but the types that it needs to be
// included in have copy constructors. Since it's a read-only value, wrapping a
// reference count allows the value to be copied and saves a lot of work
// copying the `Value` itself.
//
// The `base::Value` is also contained within a `unique_ptr` so that `values.h`,
// which is quite large, doesn't have to be included in this header file.
class COMPONENT_EXPORT(DEVICE_FIDO) JSONRequest
    : public base::RefCountedThreadSafe<JSONRequest> {
 public:
  explicit JSONRequest(base::Value json);

  const std::unique_ptr<base::Value> value;

 private:
  friend class base::RefCountedThreadSafe<JSONRequest>;
  ~JSONRequest();
};

}  // namespace device

#endif  // DEVICE_FIDO_JSON_REQUEST_H_
