// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <android/log.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#include <initializer_list>

// This defines a function Bar() which will perform the following:
//
//   - Load a first library (libfoo) with dlopen, and find the 'Foo'
//     function pointer in it.
//
//   - Load a second library (libfoo2) with dlopen, and fint the 'Foo2'
//     function pointer in it.
//
//   - Check that the list of valid dlopen handles managed by the crazy
//     linker contains the handles for the two libraries above. Note that
//     this is done by calling crazy::GetValidDlopenHandleForTesting.
//     Since this function is linked into the test executable that calls
//     this function, but not the shared library that implements it, its
//     address must be passed as the first parameter to Bar()!
//
//   - Call the 'Foo' and 'Foo2' functions.
//
//   - Close the second library handle.
//
//   - Check that the list of valid dlopen handles only contains the
//     handle for libfoo now.
//
//   - Close the first library handle.
//
//   - Check that the list of valid dlopen handles is empty now.
//
//   - Return true on success, false otherwise.

#define LIB1_NAME "libcrazy_linker_tests_libfoo.so"
#define LIB2_NAME "libcrazy_linker_tests_libfoo2.so"

// The type of the crazy::GetValidDlopenHandleForTesting() function
using GetValidHandlesFunction = void**(size_t*);

// The type of the 'Foo' and 'Foo2' functions in the two libraries
// loaded by Bar() below.
using FooFunc = void(void);

// Convenience class for a library handle, calls dlclose() on destruction
// unless Close() was called before that.
class ScopedLibHandle {
 public:
  ScopedLibHandle(void* handle) : handle_(handle) {}
  ~ScopedLibHandle() {
    if (handle_)
      dlclose(handle_);
  }
  void* Get() const { return handle_; }
  void Close() {
    if (handle_) {
      dlclose(handle_);
      handle_ = nullptr;
    }
  }

 private:
  void* handle_;
};

// Convenience class for the list of handles returned by
// crazy::GetValidDlOpenHandleForTesting().
class ScopedHandleList {
 public:
  ScopedHandleList(GetValidHandlesFunction* get_handles_func) {
    handles_ = (*get_handles_func)(&count_);
  }
  ~ScopedHandleList() { free(handles_); }

  size_t count() const { return count_; }
  void** begin() const { return handles_; }
  void** end() const { return handles_ + count_; }

 private:
  size_t count_;
  void** handles_;
};

// Convenience macro for printing errors to stderr with a file:line prefix.
#define ERROR(...)                                                 \
  do {                                                             \
    fprintf(stderr, "TEST_ERROR:%s:%d: ", __FUNCTION__, __LINE__); \
    fprintf(stderr, __VA_ARGS__);                                  \
  } while (0)

// The Bar() function in all of its glory.
extern "C" bool Bar(GetValidHandlesFunction* get_valid_handles) {
  printf("%s: Entering\n", __FUNCTION__);
  __android_log_print(ANDROID_LOG_INFO, "bar", "Hi There!");
  fprintf(stderr, "Hi There! from Bar\n");

  ScopedLibHandle lib1_handle(dlopen(LIB1_NAME, RTLD_NOW));
  if (!lib1_handle.Get()) {
    ERROR("Could not find 1st library: %s\n", dlerror());
    return false;
  }

  auto* func1 =
      reinterpret_cast<void (*)(void)>(dlsym(lib1_handle.Get(), "Foo"));
  if (!func1) {
    ERROR("Could not find 'Foo' symbol in 1st library\n");
    return false;
  }

  printf("OK: Found Foo() at %p in %s\n", func1, LIB1_NAME);

  ScopedLibHandle lib2_handle(dlopen(LIB2_NAME, RTLD_NOW));
  if (!lib2_handle.Get()) {
    ERROR("Could not find 2nd library: %s\n", dlerror());
    return false;
  }

  auto* func2 =
      reinterpret_cast<void (*)(void)>(dlsym(lib2_handle.Get(), "Foo2"));
  if (!func2) {
    ERROR("Could not find 'Foo2' symbol in 2nd library\n");
    return false;
  }

  printf("OK: Found Foo2() at %p in %s\n", func2, LIB2_NAME);

  {
    ScopedHandleList handles(get_valid_handles);
    if (handles.count() != 2) {
      ERROR("Invalid dlopen handle count (%zd expected 2)\n", handles.count());
      return false;
    }
    bool failure = false;
    int n = 1;
    for (void* handle : handles) {
      if (handle != lib1_handle.Get() && handle != lib2_handle.Get()) {
        ERROR("Invalid handle value #%d (%p expected %p or %p)\n", n, handle,
              lib1_handle.Get(), lib2_handle.Get());
        failure = true;
      }
      n++;
    }
    if (failure)
      return false;
  }

  printf("OK: Checked valid handle list, closing 2nd library\n");

  lib2_handle.Close();

  {
    ScopedHandleList handles(get_valid_handles);
    if (handles.count() != 1) {
      ERROR("Invalid dlopen handle count (%zd expected 1)\n", handles.count());
      return false;
    }
    int n = 1;
    for (void* handle : handles) {
      if (handle != lib1_handle.Get()) {
        ERROR("Invalid handle value #%d (%p expected %p)\n", n, handle,
              lib1_handle.Get());
        return false;
      }
      n++;
    }
  }

  printf("OK: Checked valid handle list, closing 1st library\n");

  lib1_handle.Close();

  {
    ScopedHandleList handles(get_valid_handles);
    if (handles.count() != 0) {
      ERROR("Invalid dlopen handle count (%zd expected 0)\n", handles.count());
      return false;
    }
  }

  printf("OK: Checked valid handle list is empty\n");

  printf("%s: Exiting\n", __FUNCTION__);
  return true;
}
