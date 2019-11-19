// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_WEB_DATABASE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_WEB_DATABASE_HOST_H_

#include <stdint.h>

#include "base/files/file.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/shared_remote.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

namespace mojom {
namespace blink {
class WebDatabaseHost;
}  // namespace blink
}  // namespace mojom

class SecurityOrigin;

// Singleton owning a remote connection to the mojo::WebDatabaseHost implementor
// in the browser process. The remote interface gets bound when initializing the
// webdatabase module (from the main thread), but the actual remote object will
// be created the first time a method gets invoked (from the Database thread).
class WebDatabaseHost {
  USING_FAST_MALLOC(WebDatabaseHost);

 public:
  static WebDatabaseHost& GetInstance();

  // Should be called once before trying to use this class, so that we make sure
  // the remote interface binding is done from the main thread before the first
  // time GetInstance() is invoked (which will happen from the Database thread).
  void Init();

  // Init() must have been invoked first before using any of the methods below.
  base::File OpenFile(const String& vfs_file_name, int desired_flags);
  int DeleteFile(const String& vfs_file_name, bool sync_dir);
  int32_t GetFileAttributes(const String& vfs_file_name);
  int64_t GetFileSize(const String& vfs_file_name);
  bool SetFileSize(const String& vfs_file_name, int64_t size);
  int64_t GetSpaceAvailableForOrigin(const SecurityOrigin& origin);

  void DatabaseOpened(const SecurityOrigin& origin,
                      const String& database_name,
                      const String& database_display_name,
                      uint32_t estimated_size);
  void DatabaseModified(const SecurityOrigin& origin,
                        const String& database_name);
  void DatabaseClosed(const SecurityOrigin& origin,
                      const String& database_name);
  void ReportSqliteError(const SecurityOrigin& origin,
                         const String& database_name,
                         int error);

 private:
  WebDatabaseHost();
  ~WebDatabaseHost() = default;

  // Returns an initialized mojom::blink::WebDatabaseHost remote. A connection
  // will be established after the first call to this method.
  mojom::blink::WebDatabaseHost& GetWebDatabaseHost();

  // Needed to bind the pending remote from the constructor, in the main thread.
  mojo::PendingRemote<mojom::blink::WebDatabaseHost> pending_remote_;

  // Need a SharedRemote as method calls will happen from the Database thread.
  mojo::SharedRemote<mojom::blink::WebDatabaseHost> shared_remote_;

  // Used to ensure that the database gets opened from the main thread, but that
  // other database-related event is reported from the database thread instead.
  scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner_;

  static WebDatabaseHost* instance_;

  DISALLOW_COPY_AND_ASSIGN(WebDatabaseHost);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBDATABASE_WEB_DATABASE_HOST_H_
