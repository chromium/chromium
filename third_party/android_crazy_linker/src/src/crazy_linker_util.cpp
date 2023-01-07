// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "crazy_linker_util.h"

#include <stdio.h>

namespace crazy {

// Return the base name from a file path. Important: this is a pointer
// into the original string.
// static
const char* GetBaseNamePtr(const char* path) {
  const char* p = strrchr(path, '/');
  if (!p)
    return path;
  else
    return p + 1;
}

// static
const char String::kEmpty[] = "";

String::String() { Init(); }

String::String(const String& other) {
  InitFrom(other.ptr_, other.size_);
}

String::String(String&& other) noexcept
    : ptr_(other.ptr_), size_(other.size_), capacity_(other.capacity_) {
  other.Init();
}

String::String(const char* str) {
  InitFrom(str, ::strlen(str));
}

String::String(char ch) {
  InitFrom(&ch, 1);
}

String::~String() {
  if (HasValidPointer()) {
    free(ptr_);
    ptr_ = const_cast<char*>(kEmpty);
  }
}

String::String(const char* str, size_t len) {
  InitFrom(str, len);
}

String& String::operator=(String&& other) noexcept {
  if (this != &other) {
    this->~String();  // Free pointer if needed.
    ptr_ = other.ptr_;
    size_ = other.size_;
    capacity_ = other.capacity_;
    other.Init();
  }
  return *this;
}

void String::Assign(const char* str, size_t len) {
  Resize(len);
  if (len > 0) {
    memcpy(ptr_, str, len);
  }
}

void String::Append(const char* str, size_t len) {
  if (len > 0) {
    size_t old_size = size_;
    Resize(size_ + len);
    memcpy(ptr_ + old_size, str, len);
  }
}

void String::Resize(size_t new_size) {
  if (new_size > capacity_) {
    size_t new_capacity = capacity_;
    while (new_capacity < new_size) {
      new_capacity += (new_capacity >> 1) + 16;
    }
    Reserve(new_capacity);
  }

  if (new_size > size_)
    memset(ptr_ + size_, '\0', new_size - size_);

  size_ = new_size;
  if (HasValidPointer()) {
    ptr_[size_] = '\0';
  }
}

void String::Reserve(size_t new_capacity) {
  char* old_ptr = HasValidPointer() ? ptr_ : nullptr;
  // Always allocate one more byte for the trailing \0
  ptr_ = reinterpret_cast<char*>(realloc(old_ptr, new_capacity + 1));
  ptr_[new_capacity] = '\0';
  capacity_ = new_capacity;

  if (size_ > new_capacity)
    size_ = new_capacity;
}

void String::InitFrom(const char* str, size_t len) {
  Init();
  if (str != nullptr && len != 0) {
    Resize(len);
    memcpy(ptr_, str, len);
  }
}

bool String::operator==(const String& other) const {
  return size_ == other.size_ && !::memcmp(ptr_, other.ptr_, size_);
}

bool String::operator==(const char* str) const {
  return !::strcmp(ptr_, str);
}

VectorBase::~VectorBase() {
  ::free(data_);
}

VectorBase::VectorBase(VectorBase&& other) noexcept
    : data_(other.data_), count_(other.count_), capacity_(other.capacity_) {
  other.DoReset();
}

VectorBase& VectorBase::operator=(VectorBase&& other) noexcept {
  if (&other != this) {
    ::free(data_);
    data_ = other.data_;
    count_ = other.count_;
    capacity_ = other.capacity_;
    other.DoReset();
  }
  return *this;
}

void VectorBase::DoResize(size_t new_count, size_t item_size) {
  if (new_count > capacity_) {
    DoReserve(new_count, item_size);
  }
  if (new_count > count_)
    ::memset(data_ + count_ * item_size, 0, (new_count - count_) * item_size);

  count_ = new_count;
}

void VectorBase::DoReserve(size_t new_capacity, size_t item_size) {
  data_ = static_cast<char*>(::realloc(data_, new_capacity * item_size));
  capacity_ = new_capacity;
  if (count_ > capacity_)
    count_ = capacity_;
}

void* VectorBase::DoInsert(size_t pos, size_t item_size) {
  return DoInsert(pos, 1, item_size);
}

void* VectorBase::DoInsert(size_t pos, size_t count, size_t item_size) {
  if (pos >= count_)
    pos = count_;

  size_t new_count = count_ + count;
  if (new_count > capacity_) {
    size_t new_capacity = capacity_;
    while (new_capacity < new_count) {
      new_capacity += (new_capacity >> 2) + 4;
    }
    DoReserve(new_capacity, item_size);
  }

  char* from_data = data_ + pos * item_size;
  char* to_data = from_data + count * item_size;

  ::memmove(to_data, from_data, (count_ - pos) * item_size);
  ::memset(from_data, 0, count * item_size);

  count_ = new_count;
  return from_data;
}

void VectorBase::DoRemove(size_t pos, size_t item_size) {
  DoRemove(pos, 1, item_size);
}

void VectorBase::DoRemove(size_t pos, size_t count, size_t item_size) {
  if (pos >= count_)
    return;

  if (count > count_ - pos)
    count = count_ - pos;

  char* to_data = data_ + pos * item_size;
  char* from_data = to_data + count * item_size;

  ::memmove(to_data, from_data, (count_ - pos - count) * item_size);
  count_ -= count;
}

}  // namespace crazy
