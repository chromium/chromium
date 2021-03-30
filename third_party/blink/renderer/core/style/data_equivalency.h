// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DATA_EQUIVALENCY_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DATA_EQUIVALENCY_H_

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "third_party/blink/renderer/platform/heap/handle.h"

namespace blink {

template <typename T>
bool DataEquivalent(const T* a, const T* b) {
  if (a == b)
    return true;
  if (!a || !b)
    return false;
  return *a == *b;
}

template <typename T>
bool DataEquivalent(const scoped_refptr<T>& a, const scoped_refptr<T>& b) {
  return DataEquivalent(a.get(), b.get());
}

template <typename T>
bool DataEquivalent(const Persistent<T>& a, const Persistent<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

template <typename T>
bool DataEquivalent(const Member<T>& a, const Member<T>& b) {
  return DataEquivalent(a.Get(), b.Get());
}

template <typename T>
bool DataEquivalent(const std::unique_ptr<T>& a, const std::unique_ptr<T>& b) {
  return DataEquivalent(a.get(), b.get());
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DATA_EQUIVALENCY_H_
