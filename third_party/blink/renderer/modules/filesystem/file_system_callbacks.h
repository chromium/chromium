/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_CALLBACKS_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_CALLBACKS_H_

#include <memory>

#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_void_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_entry_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_error_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_system_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_file_writer_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_metadata_callback.h"
#include "third_party/blink/renderer/core/fileapi/file_error.h"
#include "third_party/blink/renderer/modules/filesystem/entry_heap_vector.h"
#include "third_party/blink/renderer/platform/async_file_system_callbacks.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

class DOMFileSystem;
class DOMFileSystemBase;
class DirectoryReaderBase;
class Entry;
class ExecutionContext;
class File;
class FileMetadata;
class FileWriterBase;
class Metadata;
class ScriptPromiseResolver;

// Passed to DOMFileSystem implementations that may report errors. Subclasses
// may capture the error for throwing on return to script (for synchronous APIs)
// or call an actual script callback (for asynchronous APIs).
class ErrorCallbackBase : public GarbageCollectedFinalized<ErrorCallbackBase> {
 public:
  virtual ~ErrorCallbackBase() {}
  virtual void Trace(blink::Visitor* visitor) {}
  virtual void Invoke(base::File::Error error) = 0;
};

class FileSystemCallbacksBase : public AsyncFileSystemCallbacks {
 public:
  ~FileSystemCallbacksBase() override;

  // For ErrorCallback.
  void DidFail(base::File::Error error) final;

  // Other callback methods are implemented by each subclass.

 protected:
  FileSystemCallbacksBase(ErrorCallbackBase*,
                          DOMFileSystemBase*,
                          ExecutionContext*);

  bool ShouldScheduleCallback() const;

  // Invokes the given callback synchronously or asynchronously depending on
  // the result of |ShouldScheduleCallback|.
  template <typename CallbackMemberFunction,
            typename CallbackClass,
            typename... Args>
  void InvokeOrScheduleCallback(CallbackMemberFunction&&,
                                CallbackClass&&,
                                Args&&...);

  Persistent<ErrorCallbackBase> error_callback_;
  Persistent<DOMFileSystemBase> file_system_;
  Persistent<ExecutionContext> execution_context_;
  int async_operation_id_;
};

// Subclasses ----------------------------------------------------------------

// Wraps a script-provided callback for use in DOMFileSystem operations.
class ScriptErrorCallback final : public ErrorCallbackBase {
 public:
  static ScriptErrorCallback* Wrap(V8ErrorCallback*);
  ~ScriptErrorCallback() override {}
  void Trace(blink::Visitor*) override;

  void Invoke(base::File::Error error) override;

 private:
  explicit ScriptErrorCallback(V8ErrorCallback*);
  Member<V8PersistentCallbackInterface<V8ErrorCallback>> callback_;
};

class PromiseErrorCallback final : public ErrorCallbackBase {
 public:
  explicit PromiseErrorCallback(ScriptPromiseResolver*);
  void Trace(Visitor*) override;
  void Invoke(base::File::Error error) override;

 private:
  Member<ScriptPromiseResolver> resolver_;
};

class EntryCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidGetEntryCallback
      : public GarbageCollectedFinalized<OnDidGetEntryCallback> {
   public:
    virtual ~OnDidGetEntryCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(Entry*) = 0;

   protected:
    OnDidGetEntryCallback() = default;
  };

  class OnDidGetEntryV8Impl : public OnDidGetEntryCallback {
   public:
    static OnDidGetEntryV8Impl* Create(V8EntryCallback* callback) {
      return callback ? new OnDidGetEntryV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(Entry*) override;

   private:
    OnDidGetEntryV8Impl(V8EntryCallback* callback)
        : callback_(ToV8PersistentCallbackInterface(callback)) {}

    Member<V8PersistentCallbackInterface<V8EntryCallback>> callback_;
  };

  class OnDidGetEntryPromiseImpl : public OnDidGetEntryCallback {
   public:
    explicit OnDidGetEntryPromiseImpl(ScriptPromiseResolver*);
    void Trace(Visitor*) override;
    void OnSuccess(Entry*) override;

   private:
    Member<ScriptPromiseResolver> resolver_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      OnDidGetEntryCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DOMFileSystemBase*,
      const String& expected_path,
      bool is_directory);
  void DidSucceed() override;

 private:
  EntryCallbacks(OnDidGetEntryCallback*,
                 ErrorCallbackBase*,
                 ExecutionContext*,
                 DOMFileSystemBase*,
                 const String& expected_path,
                 bool is_directory);
  Persistent<OnDidGetEntryCallback> success_callback_;
  String expected_path_;
  bool is_directory_;
};

class EntriesCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidGetEntriesCallback
      : public GarbageCollectedFinalized<OnDidGetEntriesCallback> {
   public:
    virtual ~OnDidGetEntriesCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(EntryHeapVector*) = 0;

   protected:
    OnDidGetEntriesCallback() = default;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      OnDidGetEntriesCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DirectoryReaderBase*,
      const String& base_path);
  void DidReadDirectoryEntry(const String& name, bool is_directory) override;
  void DidReadDirectoryEntries(bool has_more) override;

 private:
  EntriesCallbacks(OnDidGetEntriesCallback*,
                   ErrorCallbackBase*,
                   ExecutionContext*,
                   DirectoryReaderBase*,
                   const String& base_path);
  Persistent<OnDidGetEntriesCallback> success_callback_;
  Persistent<DirectoryReaderBase> directory_reader_;
  String base_path_;
  Persistent<HeapVector<Member<Entry>>> entries_;
};

class FileSystemCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidOpenFileSystemCallback
      : public GarbageCollectedFinalized<OnDidOpenFileSystemCallback> {
   public:
    virtual ~OnDidOpenFileSystemCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(DOMFileSystem*) = 0;

   protected:
    OnDidOpenFileSystemCallback() = default;
  };

  class OnDidOpenFileSystemV8Impl : public OnDidOpenFileSystemCallback {
   public:
    static OnDidOpenFileSystemV8Impl* Create(V8FileSystemCallback* callback) {
      return callback ? new OnDidOpenFileSystemV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(DOMFileSystem*) override;

   private:
    OnDidOpenFileSystemV8Impl(V8FileSystemCallback* callback)
        : callback_(ToV8PersistentCallbackInterface(callback)) {}

    Member<V8PersistentCallbackInterface<V8FileSystemCallback>> callback_;
  };

  class OnDidOpenFileSystemPromiseImpl : public OnDidOpenFileSystemCallback {
   public:
    explicit OnDidOpenFileSystemPromiseImpl(ScriptPromiseResolver*);
    void Trace(Visitor*) override;
    void OnSuccess(DOMFileSystem*) override;

   private:
    Member<ScriptPromiseResolver> resolver_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      OnDidOpenFileSystemCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      mojom::blink::FileSystemType);
  void DidOpenFileSystem(const String& name, const KURL& root_url) override;

 private:
  FileSystemCallbacks(OnDidOpenFileSystemCallback*,
                      ErrorCallbackBase*,
                      ExecutionContext*,
                      mojom::blink::FileSystemType);
  Persistent<OnDidOpenFileSystemCallback> success_callback_;
  mojom::blink::FileSystemType type_;
};

class ResolveURICallbacks final : public FileSystemCallbacksBase {
 public:
  using OnDidGetEntryCallback = EntryCallbacks::OnDidGetEntryCallback;
  using OnDidGetEntryV8Impl = EntryCallbacks::OnDidGetEntryV8Impl;

  static std::unique_ptr<AsyncFileSystemCallbacks>
  Create(OnDidGetEntryCallback*, ErrorCallbackBase*, ExecutionContext*);
  void DidResolveURL(const String& name,
                     const KURL& root_url,
                     mojom::blink::FileSystemType,
                     const String& file_path,
                     bool is_directry) override;

 private:
  ResolveURICallbacks(OnDidGetEntryCallback*,
                      ErrorCallbackBase*,
                      ExecutionContext*);
  Persistent<OnDidGetEntryCallback> success_callback_;
};

class MetadataCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidReadMetadataCallback
      : public GarbageCollectedFinalized<OnDidReadMetadataCallback> {
   public:
    virtual ~OnDidReadMetadataCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(Metadata*) = 0;

   protected:
    OnDidReadMetadataCallback() = default;
  };

  class OnDidReadMetadataV8Impl : public OnDidReadMetadataCallback {
   public:
    static OnDidReadMetadataV8Impl* Create(V8MetadataCallback* callback) {
      return callback ? new OnDidReadMetadataV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(Metadata*) override;

   private:
    OnDidReadMetadataV8Impl(V8MetadataCallback* callback)
        : callback_(ToV8PersistentCallbackInterface(callback)) {}

    Member<V8PersistentCallbackInterface<V8MetadataCallback>> callback_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      OnDidReadMetadataCallback*,
      ErrorCallbackBase*,
      ExecutionContext*,
      DOMFileSystemBase*);
  void DidReadMetadata(const FileMetadata&) override;

 private:
  MetadataCallbacks(OnDidReadMetadataCallback*,
                    ErrorCallbackBase*,
                    ExecutionContext*,
                    DOMFileSystemBase*);
  Persistent<OnDidReadMetadataCallback> success_callback_;
};

class FileWriterCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidCreateFileWriterCallback
      : public GarbageCollectedFinalized<OnDidCreateFileWriterCallback> {
   public:
    virtual ~OnDidCreateFileWriterCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(FileWriterBase*) = 0;

   protected:
    OnDidCreateFileWriterCallback() = default;
  };

  class OnDidCreateFileWriterV8Impl : public OnDidCreateFileWriterCallback {
   public:
    static OnDidCreateFileWriterV8Impl* Create(V8FileWriterCallback* callback) {
      return callback ? new OnDidCreateFileWriterV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(FileWriterBase*) override;

   private:
    OnDidCreateFileWriterV8Impl(V8FileWriterCallback* callback)
        : callback_(ToV8PersistentCallbackInterface(callback)) {}

    Member<V8PersistentCallbackInterface<V8FileWriterCallback>> callback_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      FileWriterBase*,
      OnDidCreateFileWriterCallback*,
      ErrorCallbackBase*,
      ExecutionContext*);
  void DidCreateFileWriter(const KURL& path, long long length) override;

 private:
  FileWriterCallbacks(FileWriterBase*,
                      OnDidCreateFileWriterCallback*,
                      ErrorCallbackBase*,
                      ExecutionContext*);
  Persistent<FileWriterBase> file_writer_;
  Persistent<OnDidCreateFileWriterCallback> success_callback_;
};

class SnapshotFileCallback final : public FileSystemCallbacksBase {
 public:
  class OnDidCreateSnapshotFileCallback
      : public GarbageCollectedFinalized<OnDidCreateSnapshotFileCallback> {
   public:
    virtual ~OnDidCreateSnapshotFileCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(File*) = 0;

   protected:
    OnDidCreateSnapshotFileCallback() = default;
  };

  class OnDidCreateSnapshotFileV8Impl : public OnDidCreateSnapshotFileCallback {
   public:
    static OnDidCreateSnapshotFileV8Impl* Create(V8FileCallback* callback) {
      return callback ? new OnDidCreateSnapshotFileV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(File*) override;

   private:
    OnDidCreateSnapshotFileV8Impl(V8FileCallback* callback)
        : callback_(ToV8PersistentCallbackInterface(callback)) {}

    Member<V8PersistentCallbackInterface<V8FileCallback>> callback_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(
      DOMFileSystemBase*,
      const String& name,
      const KURL&,
      OnDidCreateSnapshotFileCallback*,
      ErrorCallbackBase*,
      ExecutionContext*);
  void DidCreateSnapshotFile(const FileMetadata&,
                             scoped_refptr<BlobDataHandle> snapshot) override;

 private:
  SnapshotFileCallback(DOMFileSystemBase*,
                       const String& name,
                       const KURL&,
                       OnDidCreateSnapshotFileCallback*,
                       ErrorCallbackBase*,
                       ExecutionContext*);
  String name_;
  KURL url_;
  Persistent<OnDidCreateSnapshotFileCallback> success_callback_;
};

class VoidCallbacks final : public FileSystemCallbacksBase {
 public:
  class OnDidSucceedCallback
      : public GarbageCollectedFinalized<OnDidSucceedCallback> {
   public:
    virtual ~OnDidSucceedCallback() = default;
    virtual void Trace(blink::Visitor*) {}
    virtual void OnSuccess(ExecutionContext* dummy_arg_for_sync_helper) = 0;

   protected:
    OnDidSucceedCallback() = default;
  };

  class OnDidSucceedV8Impl : public OnDidSucceedCallback {
   public:
    static OnDidSucceedV8Impl* Create(V8VoidCallback* callback) {
      return callback ? new OnDidSucceedV8Impl(callback) : nullptr;
    }
    void Trace(blink::Visitor*) override;
    void OnSuccess(ExecutionContext* dummy_arg_for_sync_helper) override;

   private:
    OnDidSucceedV8Impl(V8VoidCallback* callback)
        : callback_(ToV8PersistentCallbackInterface(callback)) {}

    Member<V8PersistentCallbackInterface<V8VoidCallback>> callback_;
  };

  class OnDidSucceedPromiseImpl : public OnDidSucceedCallback {
   public:
    OnDidSucceedPromiseImpl(ScriptPromiseResolver*);
    void Trace(Visitor*) override;
    void OnSuccess(ExecutionContext*) override;

   private:
    Member<ScriptPromiseResolver> resolver_;
  };

  static std::unique_ptr<AsyncFileSystemCallbacks> Create(OnDidSucceedCallback*,
                                                          ErrorCallbackBase*,
                                                          ExecutionContext*,
                                                          DOMFileSystemBase*);
  void DidSucceed() override;

 private:
  VoidCallbacks(OnDidSucceedCallback*,
                ErrorCallbackBase*,
                ExecutionContext*,
                DOMFileSystemBase*);
  Persistent<OnDidSucceedCallback> success_callback_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_CALLBACKS_H_
