/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2008 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DATA_REF_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DATA_REF_H_

#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

template <typename T>
class DataRef {
  DISALLOW_NEW();

 public:
  explicit DataRef(T* data) : data_(data) {}

  const T* Get() const { return data_.Get(); }

  const T& operator*() const { return *Get(); }
  const T* operator->() const { return Get(); }

  T* Access(bool& access_flag) {
    if (!access_flag) {
      access_flag = true;
      data_ = data_->Copy();
    }
    return data_.Get();
  }

  bool operator==(const DataRef<T>& o) const {
    DCHECK(data_);
    DCHECK(o.data_);
    return data_ == o.data_ || *data_ == *o.data_;
  }

  bool operator!=(const DataRef<T>& o) const {
    DCHECK(data_);
    DCHECK(o.data_);
    return data_ != o.data_ && *data_ != *o.data_;
  }

  void operator=(std::nullptr_t) { data_ = nullptr; }

  void Trace(Visitor* visitor) const { visitor->Trace(data_); }

 private:
  // These ComputedStyle sub-objects are heavily inlined, and on relatively hot
  // codepaths. Disable pointer-compression.
  subtle::UncompressedMember<T> data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_STYLE_DATA_REF_H_
