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

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_LOCAL_FILE_SYSTEM_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_LOCAL_FILE_SYSTEM_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "third_party/blink/public/mojom/filesystem/file_system.mojom-blink-forward.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/workers/worker_global_scope.h"
#include "third_party/blink/renderer/platform/bindings/name_client.h"
#include "third_party/blink/renderer/platform/heap/handle.h"
#include "third_party/blink/renderer/platform/supplementable.h"
#include "third_party/blink/renderer/platform/wtf/forward.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink {

class ExecutionContext;
class FileSystemCallbacks;
class KURL;
class ResolveURICallbacks;

class LocalFileSystem final : public GarbageCollected<LocalFileSystem>,
                              public Supplement<LocalFrame>,
                              public Supplement<WorkerGlobalScope>,
                              public NameClient {
  USING_GARBAGE_COLLECTED_MIXIN(LocalFileSystem);

 public:
  enum SynchronousType { kAsynchronous, kSynchronous };

  static const char kSupplementName[];

  explicit LocalFileSystem(LocalFrame&);
  explicit LocalFileSystem(WorkerGlobalScope&);
  ~LocalFileSystem();

  void ResolveURL(ExecutionContext*,
                  const KURL&,
                  std::unique_ptr<ResolveURICallbacks>,
                  SynchronousType sync_type);
  void RequestFileSystem(ExecutionContext*,
                         mojom::blink::FileSystemType,
                         int64_t size,
                         std::unique_ptr<FileSystemCallbacks>,
                         SynchronousType sync_type);

  static LocalFileSystem* From(ExecutionContext&);

  void Trace(blink::Visitor*) override;
  const char* NameInHeapSnapshot() const override { return "LocalFileSystem"; }

 private:
  void ResolveURLCallback(ExecutionContext* context,
                          const KURL& file_system_url,
                          std::unique_ptr<ResolveURICallbacks> callbacks,
                          SynchronousType sync_type,
                          bool allowed);
  void RequestFileSystemCallback(ExecutionContext* context,
                                 mojom::blink::FileSystemType type,
                                 std::unique_ptr<FileSystemCallbacks> callbacks,
                                 SynchronousType sync_type,
                                 bool allowed);
  void RequestFileSystemAccessInternal(ExecutionContext*,
                                       base::OnceCallback<void(bool)> callback);
  void FileSystemNotAllowedInternal(ExecutionContext*,
                                    std::unique_ptr<FileSystemCallbacks>);
  void FileSystemNotAllowedInternal(ExecutionContext*,
                                    std::unique_ptr<ResolveURICallbacks>);
  void FileSystemAllowedInternal(ExecutionContext*,
                                 mojom::blink::FileSystemType,
                                 std::unique_ptr<FileSystemCallbacks> callbacks,
                                 SynchronousType sync_type);
  void ResolveURLInternal(ExecutionContext*,
                          const KURL&,
                          std::unique_ptr<ResolveURICallbacks>,
                          SynchronousType sync_type);

  DISALLOW_COPY_AND_ASSIGN(LocalFileSystem);
};

void ProvideLocalFileSystemTo(LocalFrame&);
void ProvideLocalFileSystemToWorker(WorkerGlobalScope&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_FILESYSTEM_LOCAL_FILE_SYSTEM_H_
