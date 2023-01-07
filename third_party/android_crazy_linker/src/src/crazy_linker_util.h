// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_UTIL_H
#define CRAZY_LINKER_UTIL_H

#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>

#include <type_traits>
#include <utility>

namespace crazy {

// Helper macro to loop around EINTR errors in syscalls.
#define HANDLE_EINTR(expr) TEMP_FAILURE_RETRY(expr)

// Helper macro to tag unused variables. Use in the declaration, between
// the type and name, as in:
//     int CRAZY_UNUSED my_var = 0;
#define CRAZY_UNUSED __attribute__((unused))

// Offset in a file indicating a failure.
#define CRAZY_OFFSET_FAILED (-1)

// Helper scoped pointer class.
template <class T>
class ScopedPtr {
 public:
  ScopedPtr() : ptr_(NULL) {}
  explicit ScopedPtr(T* ptr) : ptr_(ptr) {}
  ~ScopedPtr() { Reset(NULL); }

  ScopedPtr(ScopedPtr&& other) noexcept : ptr_(other.ptr_) {
    other.ptr_ = nullptr;
  }

  ScopedPtr& operator=(ScopedPtr&& other) noexcept {
    if (this != &other) {
      delete ptr_;
      ptr_ = other.ptr_;
      other.ptr_ = nullptr;
    }
    return *this;
  }

  T* Release() {
    T* ret = ptr_;
    ptr_ = NULL;
    return ret;
  }

  void Reset(T* ptr) {
    delete ptr_;
    ptr_ = ptr;
  }

  T* Get() { return ptr_; }
  T& operator*() { return *ptr_; }
  T* operator->() { return ptr_; }

 private:
  T* ptr_;
};

// Return the base name from a file path. Important: this is a pointer
// into the original string.
const char* GetBaseNamePtr(const char* path);

// Helper class used to implement a string. Similar to std::string
// without all the crazy iterator / iostream stuff.
//
// Required because crazy linker should only link against the system
// libstdc++ that only provides new/delete.
//
class String {
 public:
  String();
  String(const char* str, size_t len);
  String(const String& other);
  String(String&& other) noexcept;

  explicit String(const char* str);
  explicit String(char ch);

  ~String();

  const char* c_str() const { return ptr_; }
  char* ptr() { return ptr_; }
  size_t size() const { return size_; }
  size_t capacity() const { return capacity_; }

  const char* begin() const { return ptr_; }
  const char* end() const { return ptr_ + size_; }

  char* begin() { return ptr_; }
  char* end() { return ptr_ + size_; }

  const char* cbegin() const { return ptr_; }
  const char* cend() const { return ptr_ + size_; }

  bool IsEmpty() const { return size_ == 0; }

  char& operator[](size_t index) { return ptr_[index]; }

  String& operator=(const String& other) {
    Assign(other.ptr_, other.size_);
    return *this;
  }

  String& operator=(String&& other) noexcept;

  String& operator=(const char* str) {
    Assign(str, strlen(str));
    return *this;
  }

  String& operator=(char ch) {
    Assign(&ch, 1);
    return *this;
  }

  String& operator+=(const String& other) {
    Append(other);
    return *this;
  }

  String& operator+=(const char* str) {
    Append(str, strlen(str));
    return *this;
  }

  String& operator+=(char ch) {
    Append(&ch, 1);
    return *this;
  }

  void Resize(size_t new_size);

  void Reserve(size_t new_capacity);

  void Assign(const char* str, size_t len);

  void Assign(const String& other) { Assign(other.ptr_, other.size_); }

  void Assign(const char* str) { Assign(str, strlen(str)); }

  void Append(const char* str, size_t len);

  void Append(const String& other) { Append(other.ptr_, other.size_); }

  void Append(const char* str) { Append(str, strlen(str)); }

  // Comparison operators.
  bool operator==(const String& other) const;
  bool operator==(const char* str) const;

  inline bool operator!=(const String& other) const {
    return !(*this == other);
  }

  inline bool operator!=(const char* str) const { return !(*this == str); }

 private:
  inline void Init() {
    ptr_ = const_cast<char*>(kEmpty);
    size_ = 0;
    capacity_ = 0;
  }

  inline bool HasValidPointer() const {
    return ptr_ != const_cast<char*>(kEmpty);
  }

  void InitFrom(const char* str, size_t len);

  static const char kEmpty[];

  char* ptr_;
  size_t size_;
  size_t capacity_;
};

// Base vector class used by all instantiations of Vector<> to reduce
// generated code size.
class VectorBase {
 public:
  VectorBase() = default;
  ~VectorBase();

  // Disallow copy operations.
  VectorBase(const VectorBase&) = delete;
  VectorBase& operator=(const VectorBase&) = delete;

  // Allow move operations.
  VectorBase(VectorBase&& other) noexcept;
  VectorBase& operator=(VectorBase&& other) noexcept;

  // Return true iff vector is empty.
  constexpr bool IsEmpty() const { return count_ == 0U; }

  // Return number of items in container.
  constexpr size_t GetCount() const { return count_; }

 protected:
  // Reset container state.
  void DoReset() {
    data_ = nullptr;
    count_ = 0;
    capacity_ = 0;
  }

  // Perform various operations on the array.
  void DoResize(size_t new_count, size_t item_size);
  void DoReserve(size_t new_capacity, size_t item_size);
  void* DoInsert(size_t pos, size_t item_size);
  void* DoInsert(size_t pos, size_t count, size_t item_size);
  void DoRemove(size_t pos, size_t item_size);
  void DoRemove(size_t pos, size_t count, size_t item_size);

  char* data_ = nullptr;
  size_t count_ = 0;
  size_t capacity_ = 0;
};

// Result type for a linear or binary search function.
// The packing generates smaller and faster machine code on ARM and x86.
struct SearchResult {
  bool found : 1;
  size_t pos : sizeof(size_t) * CHAR_BIT - 1;
};

// Helper template used to implement a simple vector of POD-struct items.
// I.e. this uses memmove() to move items during insertion / removal.
//
// Required because crazy linker should only link against the system
// libstdc++ which only provides new/delete.
//
template <class T>
class Vector : public VectorBase {
 public:
  Vector() { static_assert(std::is_pod<T>::value, "type T should be POD"); }

  ~Vector() = default;

  // Move operations are allowed.
  Vector(Vector&& other) : VectorBase(std::move(other)) {}

  Vector& operator=(Vector&& other) {
    if (this != &other) {
      this->VectorBase::operator=(std::move(other));
    }
    return *this;
  }

  // Support for-range loops.
  constexpr const T* cbegin() const {
    return reinterpret_cast<const T*>(data_);
  }
  constexpr const T* cend() const { return cbegin() + count_; }

  constexpr const T* begin() const { return cbegin(); }
  constexpr const T* end() const { return cend(); }

  T* begin() { return const_cast<T*>(cbegin()); }
  T* end() { return const_cast<T*>(cend()); }

  // Array access operator.
  constexpr const T& operator[](size_t index) const { return cbegin()[index]; }
  T& operator[](size_t index) { return begin()[index]; }

  // Append one item at the end of the vector.
  void PushBack(T item) { InsertAt(count_, item); }

  // Remove the first item from the vector and return it.
  // Undefined behaviour if it is empty.
  T PopFirst() {
    T result = cbegin()[0];
    RemoveAt(0);
    return result;
  }

  // Remove the last item from the vector and return it.
  // Undefined behaviour if the vector is empty.
  T PopLast() {
    T result = cend()[-1];
    DoResize(count_ - 1, sizeof(T));
    return result;
  }

  // Remove a specific item from the vector, if it contains it.
  void Remove(T item) {
    SearchResult result = Find(item);
    if (result.found)
      RemoveAt(result.pos);
  }

  // Insert a new |item| into a specific position |index|.
  void InsertAt(size_t index, T item) {
    auto* slot = reinterpret_cast<T*>(DoInsert(index, sizeof(T)));
    *slot = item;
  }

  // Remove the item at position |index|.
  void RemoveAt(size_t index) { DoRemove(index, sizeof(T)); }

  // Try to find |item| in the vector.
  SearchResult Find(T wanted) const {
    for (size_t pos = 0; pos < count_; ++pos) {
      if (cbegin()[pos] == wanted)
        return {true, pos};
    }
    return {false, 0};
  }

  // Returns true if vector contains |item|.
  bool Has(T item) const { return Find(item).found; }

  // Resize vector.
  void Resize(size_t new_count) { DoResize(new_count, sizeof(T)); }

  // Reset the vector's capacity to a new value. Truncate size if needed.
  void Reserve(size_t new_capacity) { DoReserve(new_capacity, sizeof(T)); }
};

}  // namespace crazy

#endif  // CRAZY_LINKER_UTIL_H
