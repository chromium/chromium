// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GIN_PUBLIC_ISOLATE_HOLDER_H_
#define GIN_PUBLIC_ISOLATE_HOLDER_H_

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "gin/gin_export.h"
#include "gin/public/v8_idle_task_runner.h"
#include "v8/include/v8.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace gin {

class PerIsolateData;
class V8IsolateMemoryDumpProvider;

// To embed Gin, first initialize gin using IsolateHolder::Initialize and then
// create an instance of IsolateHolder to hold the v8::Isolate in which you
// will execute JavaScript. You might wish to subclass IsolateHolder if you
// want to tie more state to the lifetime of the isolate.
class GIN_EXPORT IsolateHolder {
 public:
  // Controls whether or not V8 should only accept strict mode scripts.
  enum ScriptMode {
    kNonStrictMode,
    kStrictMode
  };

  // Stores whether the client uses v8::Locker to access the isolate.
  enum AccessMode {
    kSingleThread,
    kUseLocker
  };

  // Whether Atomics.wait can be called on this isolate.
  enum AllowAtomicsWaitMode {
    kDisallowAtomicsWait,
    kAllowAtomicsWait
  };

  // Indicates how the Isolate instance will be created.
  enum class IsolateCreationMode {
    kNormal,
    kCreateSnapshot,
  };

  // Isolate type used for UMA/UKM reporting:
  // - kBlinkMainThread: the main isolate of Blink.
  // - kBlinkWorkerThread: the isolate of a Blink worker.
  // - kTest: used only in tests.
  // - kUtility: the isolate of PDFium and ProxyResolver.
  enum class IsolateType {
    kBlinkMainThread,
    kBlinkWorkerThread,
    kTest,
    kUtility
  };

  explicit IsolateHolder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      IsolateType isolate_type);
  IsolateHolder(scoped_refptr<base::SingleThreadTaskRunner> task_runner,
                AccessMode access_mode,
                IsolateType isolate_type);
  IsolateHolder(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      AccessMode access_mode,
      AllowAtomicsWaitMode atomics_wait_mode,
      IsolateType isolate_type,
      IsolateCreationMode isolate_creation_mode = IsolateCreationMode::kNormal);
  ~IsolateHolder();

  // Should be invoked once before creating IsolateHolder instances to
  // initialize V8 and Gin. In case V8_USE_EXTERNAL_STARTUP_DATA is
  // defined and the snapshot file is available, it should be loaded (by calling
  // V8Initializer::LoadV8SnapshotFromFD or
  // V8Initializer::LoadV8Snapshot) before calling this method.
  // If the snapshot file contains customised contexts which have static
  // external references, |reference_table| needs to point an array of those
  // reference pointers. Otherwise, it can be nullptr.
  static void Initialize(ScriptMode mode,
                         v8::ArrayBuffer::Allocator* allocator,
                         const intptr_t* reference_table = nullptr);

  v8::Isolate* isolate() { return isolate_; }

  // This method returns if v8::Locker is needed to access isolate.
  AccessMode access_mode() const { return access_mode_; }

  IsolateType isolate_type() const { return isolate_type_; }

  v8::SnapshotCreator* snapshot_creator() const {
    return snapshot_creator_.get();
  }

  void EnableIdleTasks(std::unique_ptr<V8IdleTaskRunner> idle_task_runner);

  // This method returns V8IsolateMemoryDumpProvider of this isolate, used for
  // testing.
  V8IsolateMemoryDumpProvider* isolate_memory_dump_provider_for_testing()
      const {
    return isolate_memory_dump_provider_.get();
  }

 private:
  void SetUp(scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  std::unique_ptr<v8::SnapshotCreator> snapshot_creator_;
  v8::Isolate* isolate_;
  std::unique_ptr<PerIsolateData> isolate_data_;
  std::unique_ptr<V8IsolateMemoryDumpProvider> isolate_memory_dump_provider_;
  AccessMode access_mode_;
  IsolateType isolate_type_;

  DISALLOW_COPY_AND_ASSIGN(IsolateHolder);
};

}  // namespace gin

#endif  // GIN_PUBLIC_ISOLATE_HOLDER_H_
