// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CRAZY_LINKER_GLOBALS_H
#define CRAZY_LINKER_GLOBALS_H

#include <pthread.h>

#include "crazy_linker_library_list.h"
#include "crazy_linker_pointer_set.h"
#include "crazy_linker_rdebug.h"
#include "crazy_linker_search_path_list.h"

// All crazy linker globals are declared in this header.

namespace crazy {

class Globals {
 public:
  // Get the single Globals instance for this process.
  static Globals* Get();

  // Default constructor.
  Globals();

  // Destructor.
  ~Globals();

  // Acquire and release the mutex that protects all other non-static members.
  // ScopedLockedGlobals is recommended, to avoid using these directly.
  void Lock();
  void Unlock();

  // The list of libraries known to the crazy linker.
  LibraryList* libraries() { return &libraries_; }

  // The RDebug instance for this process.
  RDebug* rdebug() { return &rdebug_; }

  // Set of valid handles returned by the dlopen() wrapper. This is
  // required to deal with rare cases where the wrapper is passed
  // a handle that was opened with the system linker by mistake.
  PointerSet* valid_handles() { return &valid_handles_; }

  // The current library search path list used by the dlopen() wrapper.
  // Initialized from LD_LIBRARY_PATH when ::Get() creates the instance.
  SearchPathList* search_path_list() { return &search_paths_; }

  // Save JavaVM instance pointer and minimum JNI version required by this
  // client. If |java_vm| is not nullptr, it will be used to call JNI_OnLoad()
  // on every library loaded through the crazy linker, if available, and
  // JNI_UnLoad() when unloading them, respectively.
  void InitJavaVm(void* java_vm, int min_jni_version) {
    java_vm_ = java_vm;
    min_jni_version_ = min_jni_version;
  }

  // Return current JavaVM instance pointer.
  void* java_vm() const { return java_vm_; }

  // Return current minimum JNI version number.
  int minimum_jni_version() const { return min_jni_version_; }

  // Convenience function to get the global RDebug instance.
  static RDebug* GetRDebug() { return Get()->rdebug(); }

 private:
  pthread_mutex_t lock_;
  void* java_vm_ = nullptr;
  int min_jni_version_ = 0;
  LibraryList libraries_;
  SearchPathList search_paths_;
  RDebug rdebug_;
  PointerSet valid_handles_;
};

// Convenience class to retrieve the Globals instance and lock it at the same
// time on construction, then release it on destruction. Also dereference can
// be used to access global methods and members.
class ScopedLockedGlobals {
 public:
  // Default constructor acquires the lock on the global instance.
  ScopedLockedGlobals() : globals_(Globals::Get()) { globals_->Lock(); }

  // Destructor releases the lock.
  ~ScopedLockedGlobals() { globals_->Unlock(); }

  // Disallow copy operations.
  ScopedLockedGlobals(const ScopedLockedGlobals&) = delete;
  ScopedLockedGlobals& operator=(const ScopedLockedGlobals&) = delete;

  // Dereference operator.
  Globals* operator->() { return globals_; }

 private:
  Globals* globals_;
};

// Convenience class used to operate on a mutex used to synchronize access to
// the global _r_debug link map, at least from threads using the crazy linker.
class ScopedLinkMapLocker {
 public:
  ScopedLinkMapLocker() { pthread_mutex_lock(&s_lock_); }
  ~ScopedLinkMapLocker() { pthread_mutex_unlock(&s_lock_); }

 private:
  static pthread_mutex_t s_lock_;
};

}  // namespace crazy

#endif  // CRAZY_LINKER_GLOBALS_H
