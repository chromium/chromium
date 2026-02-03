// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sql/test/test_vfs.h"

#include <stddef.h>

#include <memory>
#include <type_traits>
#include <utility>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/memory/raw_ref.h"
#include "sql/initialization.h"
#include "third_party/sqlite/sqlite3.h"

namespace sql::test {
namespace {

// Functor for deleting memory allocated by SQLite.
struct SqliteMemoryDeleter {
  void operator()(void* ptr) const { sqlite3_free(ptr); }
};

// An `std::unique_ptr` that deletes objects using `sqlite3_free`.
template <typename T>
using UniqueSqlitePtr = std::unique_ptr<T, SqliteMemoryDeleter>;

// A file created by `TestVfs`.
//
// `TestVfsFile` is a C-style child class of `sqlite3_file` (i.e. it's safe to
// cast `TestVfsFile*` to `sqlite3_file*`), providing SQLite with callbacks to
// invoke to do file IOs. These callbacks simply forward the calls to the
// `TestVfs` that created this file, which then forward the call back to the
// original `sqlite3_io_methods` of the file that this `TestVfsFile` wraps.
// `TestVfs` uses virtual methods, allowing unit tests to customize all VFS and
// file IO operations in a central place.
class TestVfsFile {
 public:
  // Constructs an instance of `TestVfsFile` in the memory reserved in `file`.
  // This memory is pre-allocated by SQLite when calling `xOpen`.
  static void InplaceCreate(sqlite3_file* file,
                            UniqueSqlitePtr<sqlite3_file> wrapped_file,
                            TestVfs& test_vfs) {
    TestVfsFile* test_vfs_file = CastFrom(file);
    new (test_vfs_file) TestVfsFile(std::move(wrapped_file), test_vfs);
  }

  // Destroys the `TestVfsFile` object in `file`.
  static void Destroy(sqlite3_file* file) {
    TestVfsFile* test_vfs_file = CastFrom(file);
    test_vfs_file->~TestVfsFile();
  }

  // Casts from `sqlite3_file*` to `TestVfsFile*`. This is safe because
  // `TestVfsFile` has a standard memory layout and its first member is an
  // instance of `sqlite3_file`.
  static TestVfsFile* CastFrom(sqlite3_file* file) {
    static_assert(std::is_standard_layout_v<TestVfsFile>,
                  "Needed for the reinterpret_cast below");
    static_assert(offsetof(TestVfsFile, file_) == 0,
                  "file_ must be the first member of the class");
    return reinterpret_cast<TestVfsFile*>(file);
  }

  sqlite3_file* wrapped_file() const { return wrapped_file_.get(); }

  TestVfs& test_vfs() const { return test_vfs_.get(); }

 private:
  TestVfsFile(UniqueSqlitePtr<sqlite3_file> wrapped_file, TestVfs& test_vfs)
      : file_({.pMethods = GetSqliteIoMethods()}),
        wrapped_file_(std::move(wrapped_file)),
        test_vfs_(test_vfs) {}

  // Returns a pointer to a static `sqlite3_io_methods` instance, telling SQLite
  // what callback to call for file IOs.
  static const sqlite3_io_methods* GetSqliteIoMethods();

  // C-style base class, `file_` must be the first member in the class.
  const sqlite3_file file_;

  // The `sqlite3_file` from the original VFS that this `TestVfsFile` wraps.
  const UniqueSqlitePtr<sqlite3_file> wrapped_file_;

  // The TestVfs that created this TestVfsFile.
  const raw_ref<TestVfs> test_vfs_;
};

// Returns the `TestVfs` instance associated with the given SQLite object.
TestVfs& GetTestVfs(sqlite3_vfs* vfs) {
  return CHECK_DEREF(reinterpret_cast<TestVfs*>(vfs->pAppData));
}
TestVfs& GetTestVfs(sqlite3_file* file) {
  return TestVfsFile::CastFrom(file)->test_vfs();
}

// Defines a C-compatible function `Callback` forwarding the call to the
// `TestVfs` method pointer `callback`.
template <auto callback>
struct ForwardTo;

template <typename Return,
          typename SqliteObject,
          typename... Args,
          Return (TestVfs::*callback)(SqliteObject* obj, Args... args)>
struct ForwardTo<callback> {
  static Return Callback(SqliteObject* obj, Args... args) {
    return (GetTestVfs(obj).*callback)(obj, args...);
  }
};

const sqlite3_io_methods* TestVfsFile::GetSqliteIoMethods() {
  static const sqlite3_io_methods kIoMethods = {
      // VFS IO API entry points are listed at:
      // https://www.sqlite.org/c3ref/io_methods.html
      .iVersion = 3,
      .xClose = ForwardTo<&TestVfs::Close>::Callback,
      .xRead = ForwardTo<&TestVfs::Read>::Callback,
      .xWrite = ForwardTo<&TestVfs::Write>::Callback,
      .xTruncate = ForwardTo<&TestVfs::Truncate>::Callback,
      .xSync = ForwardTo<&TestVfs::Sync>::Callback,
      .xFileSize = ForwardTo<&TestVfs::FileSize>::Callback,
      .xLock = ForwardTo<&TestVfs::Lock>::Callback,
      .xUnlock = ForwardTo<&TestVfs::Unlock>::Callback,
      .xCheckReservedLock = ForwardTo<&TestVfs::CheckReservedLock>::Callback,
      .xFileControl = ForwardTo<&TestVfs::FileControl>::Callback,
      .xSectorSize = ForwardTo<&TestVfs::SectorSize>::Callback,
      .xDeviceCharacteristics =
          ForwardTo<&TestVfs::DeviceCharacteristics>::Callback,
      .xShmMap = ForwardTo<&TestVfs::ShmMap>::Callback,
      .xShmLock = ForwardTo<&TestVfs::ShmLock>::Callback,
      .xShmBarrier = ForwardTo<&TestVfs::ShmBarrier>::Callback,
      .xShmUnmap = ForwardTo<&TestVfs::ShmUnmap>::Callback,
      .xFetch = ForwardTo<&TestVfs::Fetch>::Callback,
      .xUnfetch = ForwardTo<&TestVfs::Unfetch>::Callback,
  };

  return &kIoMethods;
}

}  // namespace

TestVfs::TestVfs()
    : vfs_({
          // VFS API entry points are listed at:
          // https://www.sqlite.org/c3ref/vfs.html
          .iVersion = 3,
          .szOsFile = sizeof(TestVfsFile),
          .mxPathname = 512,
          .pNext = nullptr,
          .zName = TestVfs::kName,
          .pAppData = this,
          .xOpen = ForwardTo<&TestVfs::Open>::Callback,
          .xDelete = ForwardTo<&TestVfs::Delete>::Callback,
          .xAccess = ForwardTo<&TestVfs::Access>::Callback,
          .xFullPathname = ForwardTo<&TestVfs::FullPathname>::Callback,
          .xDlOpen = nullptr,
          .xDlError = nullptr,
          .xDlSym = nullptr,
          .xDlClose = nullptr,
          .xRandomness = ForwardTo<&TestVfs::Randomness>::Callback,
          .xSleep = ForwardTo<&TestVfs::Sleep>::Callback,
          .xCurrentTime = nullptr,
          .xGetLastError = ForwardTo<&TestVfs::GetLastError>::Callback,
          .xCurrentTimeInt64 = ForwardTo<&TestVfs::CurrentTimeInt64>::Callback,
          .xSetSystemCall = nullptr,
          .xGetSystemCall = nullptr,
          .xNextSystemCall = nullptr,
      }) {
  // Register `VfsWrapper` first, so that `TestVfs` wraps over `VfsWrapper`.
  // `VfsWrapper` is needed for the VFS to work on all platforms (e.g. Fuchsia).
  // `VfsWrapper` cannot wrap `TestVfs` because it's designed to wrap the
  // platform's default VFS directly (e.g. for Fuchsia, it explicitly wraps
  // "unix-none" instead of the default VFS).
  EnsureSqliteInitialized();

  CHECK_EQ(sqlite3_vfs_find(TestVfs::kName), nullptr)
      << "Two instances of TestVfs can't be live at the same time.";

  original_vfs_ = sqlite3_vfs_find(nullptr);  // Lookup default VFS.
  CHECK(original_vfs_) << "No default VFS found.";

  int rc = sqlite3_vfs_register(&vfs_, /*makeDflt=*/true);
  CHECK_EQ(rc, SQLITE_OK);
}

TestVfs::~TestVfs() {
  CHECK_EQ(sqlite3_vfs_unregister(&vfs_), SQLITE_OK);

  // Restore the original default because `sqlite3_vfs_unregister` choses an
  // arbitrary VFS as default.
  CHECK_EQ(sqlite3_vfs_register(original_vfs_, /*makeDflt=*/true), SQLITE_OK);
}

int TestVfs::Open(sqlite3_vfs* vfs,
                  const char* full_path,
                  sqlite3_file* result_file,
                  int requested_flags,
                  int* granted_flags) {
  UniqueSqlitePtr<sqlite3_file> wrapped_file(
      reinterpret_cast<sqlite3_file*>(sqlite3_malloc(original_vfs_->szOsFile)));
  if (!wrapped_file) {
    return SQLITE_NOMEM;
  }

  int rc = original_vfs_->xOpen(original_vfs_, full_path, wrapped_file.get(),
                                requested_flags, granted_flags);
  if (rc != SQLITE_OK) {
    return rc;
  }

  TestVfsFile::InplaceCreate(result_file, std::move(wrapped_file), *this);
  return rc;
}

int TestVfs::Delete(sqlite3_vfs* vfs, const char* full_path, int sync_dir) {
  return original_vfs_->xDelete(original_vfs_, full_path, sync_dir);
}

int TestVfs::Access(sqlite3_vfs* vfs,
                    const char* full_path,
                    int flags,
                    int* result) {
  return original_vfs_->xAccess(original_vfs_, full_path, flags, result);
}

int TestVfs::FullPathname(sqlite3_vfs* vfs,
                          const char* file_path,
                          int result_size,
                          char* result) {
  return original_vfs_->xFullPathname(original_vfs_, file_path, result_size,
                                      result);
}

int TestVfs::Randomness(sqlite3_vfs* vfs, int result_size, char* result) {
  return original_vfs_->xRandomness(original_vfs_, result_size, result);
}

int TestVfs::Sleep(sqlite3_vfs* vfs, int microseconds) {
  return original_vfs_->xSleep(original_vfs_, microseconds);
}

int TestVfs::GetLastError(sqlite3_vfs* vfs, int message_size, char* message) {
  return original_vfs_->xGetLastError(original_vfs_, message_size, message);
}

int TestVfs::CurrentTimeInt64(sqlite3_vfs* vfs, sqlite3_int64* result_ms) {
  return original_vfs_->xCurrentTimeInt64(original_vfs_, result_ms);
}

int TestVfs::Close(sqlite3_file* file) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  int result = wrapped_file->pMethods->xClose(wrapped_file);
  TestVfsFile::Destroy(file);
  return result;
}

int TestVfs::Read(sqlite3_file* file,
                  void* buffer,
                  int size,
                  sqlite3_int64 offset) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xRead(wrapped_file, buffer, size, offset);
}

int TestVfs::Write(sqlite3_file* file,
                   const void* buffer,
                   int size,
                   sqlite3_int64 offset) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xWrite(wrapped_file, buffer, size, offset);
}

int TestVfs::Truncate(sqlite3_file* file, sqlite3_int64 size) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xTruncate(wrapped_file, size);
}

int TestVfs::Sync(sqlite3_file* file, int flags) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xSync(wrapped_file, flags);
}

int TestVfs::FileSize(sqlite3_file* file, sqlite3_int64* result_size) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xFileSize(wrapped_file, result_size);
}

int TestVfs::Lock(sqlite3_file* file, int mode) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xLock(wrapped_file, mode);
}

int TestVfs::Unlock(sqlite3_file* file, int mode) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xUnlock(wrapped_file, mode);
}

int TestVfs::CheckReservedLock(sqlite3_file* file, int* has_reserved_lock) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xCheckReservedLock(wrapped_file,
                                                    has_reserved_lock);
}

int TestVfs::FileControl(sqlite3_file* file, int opcode, void* data) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xFileControl(wrapped_file, opcode, data);
}

int TestVfs::SectorSize(sqlite3_file* file) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xSectorSize(wrapped_file);
}

int TestVfs::DeviceCharacteristics(sqlite3_file* file) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xDeviceCharacteristics(wrapped_file);
}

int TestVfs::ShmMap(sqlite3_file* file,
                    int page_index,
                    int page_size,
                    int extend_file_if_needed,
                    void volatile** result) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xShmMap(wrapped_file, page_index, page_size,
                                         extend_file_if_needed, result);
}

int TestVfs::ShmLock(sqlite3_file* file, int offset, int size, int flags) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xShmLock(wrapped_file, offset, size, flags);
}

void TestVfs::ShmBarrier(sqlite3_file* file) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xShmBarrier(wrapped_file);
}

int TestVfs::ShmUnmap(sqlite3_file* file, int also_delete_file) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xShmUnmap(wrapped_file, also_delete_file);
}

int TestVfs::Fetch(sqlite3_file* file,
                   sqlite3_int64 offset,
                   int size,
                   void** result) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xFetch(wrapped_file, offset, size, result);
}

int TestVfs::Unfetch(sqlite3_file* file,
                     sqlite3_int64 offset,
                     void* fetch_result) {
  sqlite3_file* wrapped_file = TestVfsFile::CastFrom(file)->wrapped_file();
  return wrapped_file->pMethods->xUnfetch(wrapped_file, offset, fetch_result);
}

}  // namespace sql::test
