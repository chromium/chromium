// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DIRECTORY_ITERATOR_H_

#include "base/files/file.h"
#include "third_party/blink/renderer/modules/filesystem/directory_reader_base.h"
#include "third_party/blink/renderer/modules/filesystem/dom_file_system.h"

namespace blink {
class ScriptPromise;
class ScriptPromiseResolver;
class ScriptState;

class FileSystemDirectoryIterator : public DirectoryReaderBase {
  DEFINE_WRAPPERTYPEINFO();
 public:
  FileSystemDirectoryIterator(DOMFileSystemBase*, const String& full_path);

  ScriptPromise next(ScriptState*);
  // TODO(mek): This return method should cancel the backend directory iteration
  // operation, to avoid doing useless work.
  void IteratorReturn() {}

  void Trace(Visitor*) override;

 private:
  class EntriesCallbackHelper;
  class ErrorCallbackHelper;
  void AddEntries(const EntryHeapVector& entries);
  void OnError(base::File::Error);

  base::File::Error error_ = base::File::FILE_OK;
  HeapDeque<Member<Entry>> entries_;
  Member<ScriptPromiseResolver> pending_next_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_FILE_SYSTEM_DIRECTORY_ITERATOR_H_
