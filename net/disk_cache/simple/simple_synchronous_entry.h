// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
#define NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_

#include <stdint.h>

#include <algorithm>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "net/base/cache_type.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/disk_cache/simple/simple_entry_format.h"
#include "net/disk_cache/simple/simple_file_tracker.h"
#include "net/disk_cache/simple/simple_histogram_enums.h"

namespace net {
class GrowableIOBuffer;
class IOBuffer;
}

FORWARD_DECLARE_TEST(DiskCacheBackendTest, SimpleCacheEnumerationLongKeys);

namespace disk_cache {

class BackendFileOperations;
class UnboundBackendFileOperations;

NET_EXPORT_PRIVATE BASE_DECLARE_FEATURE(kSimpleCachePrefetchExperiment);
NET_EXPORT_PRIVATE extern const char kSimpleCacheFullPrefetchBytesParam[];
NET_EXPORT_PRIVATE extern const char
    kSimpleCacheTrailerPrefetchSpeculativeBytesParam[];

// Returns how large a file would get prefetched on reading the entry.
// If the experiment is disabled, returns 0.
NET_EXPORT_PRIVATE int GetSimpleCachePrefetchSize();

class SimpleSynchronousEntry;
struct RangeResult;

// This class handles the passing of data about the entry between
// SimpleEntryImplementation and SimpleSynchronousEntry and the computation of
// file offsets based on the data size for all streams.
class NET_EXPORT_PRIVATE SimpleEntryStat {
 public:
  SimpleEntryStat(base::Time last_used,
                  base::Time last_modified,
                  const int32_t data_size[],
                  const int32_t sparse_data_size);

  int GetOffsetInFile(size_t key_length, int offset, int stream_index) const;
  int GetEOFOffsetInFile(size_t key_length, int stream_index) const;
  int GetLastEOFOffsetInFile(size_t key_length, int file_index) const;
  int64_t GetFileSize(size_t key_length, int file_index) const;

  base::Time last_used() const { return last_used_; }
  base::Time last_modified() const { return last_modified_; }
  void set_last_used(base::Time last_used) { last_used_ = last_used; }
  void set_last_modified(base::Time last_modified) {
    last_modified_ = last_modified;
  }

  int32_t data_size(int stream_index) const { return data_size_[stream_index]; }
  void set_data_size(int stream_index, int data_size) {
    data_size_[stream_index] = data_size;
  }

  int32_t sparse_data_size() const { return sparse_data_size_; }
  void set_sparse_data_size(int32_t sparse_data_size) {
    sparse_data_size_ = sparse_data_size;
  }

 private:
  base::Time last_used_;
  base::Time last_modified_;
  int32_t data_size_[kSimpleEntryStreamCount];
  int32_t sparse_data_size_;
};

struct SimpleStreamPrefetchData {
  SimpleStreamPrefetchData();
  ~SimpleStreamPrefetchData();

  scoped_refptr<net::GrowableIOBuffer> data;
  uint32_t stream_crc32;
};

struct SimpleEntryCreationResults {
  explicit SimpleEntryCreationResults(SimpleEntryStat entry_stat);
  ~SimpleEntryCreationResults();

  raw_ptr<SimpleSynchronousEntry> sync_entry;
  // This is set when `sync_entry` is null.
  std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations;

  // Expectation is that [0] will always be filled in, but [1] might not be.
  SimpleStreamPrefetchData stream_prefetch_data[2];

  SimpleEntryStat entry_stat;
  int32_t computed_trailer_prefetch_size = -1;
  int result = net::OK;
  bool created = false;
};

struct SimpleEntryCloseResults {
  int32_t estimated_trailer_prefetch_size = -1;
};

// Worker thread interface to the very simple cache. This interface is not
// thread safe, and callers must ensure that it is only ever accessed from
// a single thread between synchronization points.
class SimpleSynchronousEntry {
 public:
  struct CRCRecord {
    CRCRecord();
    CRCRecord(int index_p, bool has_crc32_p, uint32_t data_crc32_p);

    int index;
    bool has_crc32;
    uint32_t data_crc32;
  };

  struct ReadRequest {
    // Also sets request_update_crc to false.
    ReadRequest(int index_p, int offset_p, int buf_len_p);
    int index;
    int offset;
    int buf_len;

    // Partial CRC of data immediately preceeding this read. Only relevant if
    // request_update_crc is set.
    uint32_t previous_crc32;
    bool request_update_crc = false;
    bool request_verify_crc;  // only relevant if request_update_crc is set
  };

  struct ReadResult {
    ReadResult() = default;
    int result;
    uint32_t updated_crc32;  // only relevant if crc_updated set
    bool crc_updated = false;
  };

  struct WriteRequest {
    WriteRequest(int index_p,
                 int offset_p,
                 int buf_len_p,
                 uint32_t previous_crc32_p,
                 bool truncate_p,
                 bool doomed_p,
                 bool request_update_crc_p);
    int index;
    int offset;
    int buf_len;
    uint32_t previous_crc32;
    bool truncate;
    bool doomed;
    bool request_update_crc;
  };

  struct WriteResult {
    WriteResult() = default;
    int result;
    uint32_t updated_crc32;  // only relevant if crc_updated set
    bool crc_updated = false;
  };

  struct SparseRequest {
    SparseRequest(int64_t sparse_offset_p, int buf_len_p);

    int64_t sparse_offset;
    int buf_len;
  };

  NET_EXPORT_PRIVATE SimpleSynchronousEntry(
      net::CacheType cache_type,
      const base::FilePath& path,
      const std::optional<std::string>& key,
      uint64_t entry_hash,
      SimpleFileTracker* simple_file_tracker,
      std::unique_ptr<UnboundBackendFileOperations> file_operations,
      int32_t stream_0_size);

  // Like Entry, the SimpleSynchronousEntry self releases when Close() is
  // called, but sometimes temporary ones are kept in unique_ptr.
  NET_EXPORT_PRIVATE ~SimpleSynchronousEntry();

  // Opens a disk cache entry on disk. The |key| parameter is optional, if empty
  // the operation may be slower. The |entry_hash| parameter is required.
  static void OpenEntry(
      net::CacheType cache_type,
      const base::FilePath& path,
      const std::optional<std::string>& key,
      uint64_t entry_hash,
      SimpleFileTracker* file_tracker,
      std::unique_ptr<UnboundBackendFileOperations> file_operations,
      int32_t trailer_prefetch_size,
      SimpleEntryCreationResults* out_results);

  static void CreateEntry(
      net::CacheType cache_type,
      const base::FilePath& path,
      const std::string& key,
      uint64_t entry_hash,
      SimpleFileTracker* file_tracker,
      std::unique_ptr<UnboundBackendFileOperations> file_operations,
      SimpleEntryCreationResults* out_results);

  static void OpenOrCreateEntry(
      net::CacheType cache_type,
      const base::FilePath& path,
      const std::string& key,
      uint64_t entry_hash,
      OpenEntryIndexEnum index_state,
      bool optimistic_create,
      SimpleFileTracker* file_tracker,
      std::unique_ptr<UnboundBackendFileOperations> file_operations,
      int32_t trailer_prefetch_size,
      SimpleEntryCreationResults* out_results);

  // Renames the entry on the file system, making it no longer possible to open
  // it again, but allowing operations to continue to be executed through that
  // instance. The renamed file will be removed once the entry is closed.
  // Returns a net error code.
  int Doom();

  // Deletes an entry from the file system.  This variant should only be used
  // if there is no actual open instance around, as it doesn't account for
  // possibility of it having been renamed to a non-standard name.
  static int DeleteEntryFiles(
      const base::FilePath& path,
      net::CacheType cache_type,
      uint64_t entry_hash,
      std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations);

  // Like |DeleteEntryFiles()| above, except that it truncates the entry files
  // rather than deleting them. Used when dooming entries after the backend has
  // shutdown. See implementation of |SimpleEntryImpl::DoomEntryInternal()| for
  // more.
  static int TruncateEntryFiles(
      const base::FilePath& path,
      uint64_t entry_hash,
      std::unique_ptr<UnboundBackendFileOperations> file_operations);

  // Like |DeleteEntryFiles()| above. Deletes all entries corresponding to the
  // |key_hashes|. Succeeds only when all entries are deleted. Returns a net
  // error code.
  static int DeleteEntrySetFiles(
      const std::vector<uint64_t>* key_hashes,
      const base::FilePath& path,
      std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations);

  // N.B. ReadData(), WriteData(), CheckEOFRecord(), ReadSparseData(),
  // WriteSparseData() and Close() may block on IO.
  //
  // All of these methods will put the //net return value into |*out_result|.

  void ReadData(const ReadRequest& in_entry_op,
                SimpleEntryStat* entry_stat,
                net::IOBuffer* out_buf,
                ReadResult* out_result);
  void WriteData(const WriteRequest& in_entry_op,
                 net::IOBuffer* in_buf,
                 SimpleEntryStat* out_entry_stat,
                 WriteResult* out_write_result);
  int CheckEOFRecord(BackendFileOperations* file_operations,
                     base::File* file,
                     int stream_index,
                     const SimpleEntryStat& entry_stat,
                     uint32_t expected_crc32);

  void ReadSparseData(const SparseRequest& in_entry_op,
                      net::IOBuffer* out_buf,
                      base::Time* out_last_used,
                      int* out_result);
  void WriteSparseData(const SparseRequest& in_entry_op,
                       net::IOBuffer* in_buf,
                       uint64_t max_sparse_data_size,
                       SimpleEntryStat* out_entry_stat,
                       int* out_result);
  void GetAvailableRange(const SparseRequest& in_entry_op,
                         RangeResult* out_result);

  // Close all streams, and add write EOF records to streams indicated by the
  // CRCRecord entries in |crc32s_to_write|.
  void Close(const SimpleEntryStat& entry_stat,
             std::unique_ptr<std::vector<CRCRecord>> crc32s_to_write,
             net::GrowableIOBuffer* stream_0_data,
             SimpleEntryCloseResults* out_results);

  const base::FilePath& path() const { return path_; }
  std::optional<std::string> key() const { return key_; }
  const SimpleFileTracker::EntryFileKey& entry_file_key() const {
    return entry_file_key_;
  }

  NET_EXPORT_PRIVATE base::FilePath GetFilenameForSubfile(
      SimpleFileTracker::SubFile sub_file) const;

  int32_t computed_trailer_prefetch_size() const {
    return computed_trailer_prefetch_size_;
  }

 private:
  FRIEND_TEST_ALL_PREFIXES(::DiskCacheBackendTest,
                           SimpleCacheEnumerationLongKeys);
  friend class SimpleFileTrackerTest;
  class PrefetchData;
  class ScopedFileOperationsBinding;

  enum FileRequired {
    FILE_NOT_REQUIRED,
    FILE_REQUIRED
  };

  struct SparseRange {
    int64_t offset;
    int64_t length;
    uint32_t data_crc32;
    int64_t file_offset;

    bool operator<(const SparseRange& other) const {
      return offset < other.offset;
    }
  };

  // When opening an entry without knowing the key, the header must be read
  // without knowing the size of the key. This is how much to read initially, to
  // make it likely the entire key is read.
  static const size_t kInitialHeaderRead = 64 * 1024;

  // Tries to open one of the cache entry files. Succeeds if the open succeeds
  // or if the file was not found and is allowed to be omitted if the
  // corresponding stream is empty.
  bool MaybeOpenFile(BackendFileOperations* file_operations,
                     int file_index,
                     base::File::Error* out_error);
  // Creates one of the cache entry files if necessary. If the file is allowed
  // to be omitted if the corresponding stream is empty, and if |file_required|
  // is FILE_NOT_REQUIRED, then the file is not created; otherwise, it is.
  bool MaybeCreateFile(BackendFileOperations* file_operations,
                       int file_index,
                       FileRequired file_required,
                       base::File::Error* out_error);
  bool OpenFiles(BackendFileOperations* file_operations,
                 SimpleEntryStat* out_entry_stat);
  bool CreateFiles(BackendFileOperations* file_operations,
                   SimpleEntryStat* out_entry_stat);
  void CloseFile(BackendFileOperations* file_operations, int index);
  void CloseFiles();

  // Read the header and key at the beginning of the file, and validate that
  // they are correct. If this entry was opened with a key, the key is checked
  // for a match. If not, then the |key_| member is set based on the value in
  // this header. Records histograms if any check is failed.
  bool CheckHeaderAndKey(base::File* file, int file_index);

  // Returns a net error, i.e. net::OK on success.
  int InitializeForOpen(BackendFileOperations* file_operations,
                        SimpleEntryStat* out_entry_stat,
                        SimpleStreamPrefetchData stream_prefetch_data[2]);

  // Writes the header and key to a newly-created stream file. |index| is the
  // index of the stream. Returns true on success; returns false and failure.
  bool InitializeCreatedFile(BackendFileOperations* file_operations, int index);

  // Returns a net error, including net::OK on success and net::FILE_EXISTS
  // when the entry already exists.
  int InitializeForCreate(BackendFileOperations* file_operations,
                          SimpleEntryStat* out_entry_stat);

  // Allocates and fills a buffer with stream 0 data in |stream_0_data|, then
  // checks its crc32. May also optionally read in |stream_1_data| and its
  // crc, but might decide not to.
  int ReadAndValidateStream0AndMaybe1(
      BackendFileOperations* file_operations,
      int file_size,
      SimpleEntryStat* out_entry_stat,
      SimpleStreamPrefetchData stream_prefetch_data[2]);

  // Reads the EOF record located at |file_offset| in file |file_index|,
  // with |file_0_prefetch| potentially having prefetched file 0 content.
  // Puts the result into |*eof_record| and sanity-checks it.
  // Returns net status, and records any failures to UMA.
  int GetEOFRecordData(base::File* file,
                       PrefetchData* prefetch_data,
                       int file_index,
                       int file_offset,
                       SimpleFileEOF* eof_record);

  // Reads either from |file_0_prefetch| or |file|.
  // Range-checks all the in-memory reads.
  bool ReadFromFileOrPrefetched(base::File* file,
                                PrefetchData* prefetch_data,
                                int file_index,
                                int offset,
                                int size,
                                char* dest);

  // Extracts out the payload of stream |stream_index|, reading either from
  // |file_0_prefetch|, if available, or |file|. |entry_stat| will be used to
  // determine file layout, though |extra_size| additional bytes will be read
  // past the stream payload end.
  //
  // |*stream_data| will be pointed to a fresh buffer with the results,
  // and |*out_crc32| will get the checksum, which will be verified against
  // |eof_record|.
  int PreReadStreamPayload(base::File* file,
                           PrefetchData* prefetch_data,
                           int stream_index,
                           int extra_size,
                           const SimpleEntryStat& entry_stat,
                           const SimpleFileEOF& eof_record,
                           SimpleStreamPrefetchData* out);

  // Opens the sparse data file and scans it if it exists.
  bool OpenSparseFileIfExists(BackendFileOperations* file_operations,
                              int32_t* out_sparse_data_size);

  // Creates and initializes the sparse data file.
  bool CreateSparseFile(BackendFileOperations* file_operations);

  // Closes the sparse data file.
  void CloseSparseFile(BackendFileOperations* file_operations);

  // Writes the header to the (newly-created) sparse file.
  bool InitializeSparseFile(base::File* file);

  // Removes all but the header of the sparse file.
  bool TruncateSparseFile(base::File* sparse_file);

  // Scans the existing ranges in the sparse file. Populates |sparse_ranges_|
  // and sets |*out_sparse_data_size| to the total size of all the ranges (not
  // including headers).
  bool ScanSparseFile(base::File* sparse_file, int32_t* out_sparse_data_size);

  // Reads from a single sparse range. If asked to read the entire range, also
  // verifies the CRC32.
  bool ReadSparseRange(base::File* sparse_file,
                       const SparseRange* range,
                       int offset,
                       int len,
                       char* buf);

  // Writes to a single (existing) sparse range. If asked to write the entire
  // range, also updates the CRC32; otherwise, invalidates it.
  bool WriteSparseRange(base::File* sparse_file,
                        SparseRange* range,
                        int offset,
                        int len,
                        const char* buf);

  // Appends a new sparse range to the sparse data file.
  bool AppendSparseRange(base::File* sparse_file,
                         int64_t offset,
                         int len,
                         const char* buf);

  static int DeleteEntryFilesInternal(const base::FilePath& path,
                                      net::CacheType cache_type,
                                      uint64_t entry_hash,
                                      BackendFileOperations* file_operations);

  static bool DeleteFileForEntryHash(const base::FilePath& path,
                                     uint64_t entry_hash,
                                     int file_index,
                                     BackendFileOperations* file_operations);
  static bool DeleteFilesForEntryHash(const base::FilePath& path,
                                      uint64_t entry_hash,
                                      BackendFileOperations* file_operations);
  static bool TruncateFilesForEntryHash(const base::FilePath& path,
                                        uint64_t entry_hash,
                                        BackendFileOperations* file_operations);

  int DoomInternal(BackendFileOperations* file_operations);

  base::FilePath GetFilenameFromFileIndex(int file_index) const;

  bool sparse_file_open() const { return sparse_file_open_; }

  const net::CacheType cache_type_;
  const base::FilePath path_;
  SimpleFileTracker::EntryFileKey entry_file_key_;
  std::optional<std::string> key_;

  bool have_open_files_ = false;
  bool initialized_ = false;

  // Normally false. This is set to true when an entry is opened without
  // checking the file headers. Any subsequent read will perform the check
  // before completing.
  bool header_and_key_check_needed_[kSimpleEntryNormalFileCount] = {
      false,
  };

  raw_ptr<SimpleFileTracker> file_tracker_;

  // An interface to allow file operations. This is in an "unbound" state
  // because each operation can run on different sequence from each other.
  // Each operation can convert this to a BackendFileOperations with calling
  // Bind(), relying on that at most one operation runs at a time.
  std::unique_ptr<UnboundBackendFileOperations> unbound_file_operations_;

  // The number of trailing bytes in file 0 that we believe should be
  // prefetched in order to read the EOF record and stream 0.  This is
  // a hint from the index and may not be exactly right.  -1 if we
  // don't have a hinted value.
  int32_t trailer_prefetch_size_;

  // The exact number of trailing bytes that were needed to read the
  // EOF record and stream 0 when the entry was actually opened.  This
  // may be different from the trailer_prefetch_size_ hint and is
  // propagated back to the index in order to optimize the next open.
  int32_t computed_trailer_prefetch_size_ = -1;

  // True if the corresponding stream is empty and therefore no on-disk file
  // was created to store it.
  bool empty_file_omitted_[kSimpleEntryNormalFileCount];

  typedef std::map<int64_t, SparseRange> SparseRangeOffsetMap;
  typedef SparseRangeOffsetMap::iterator SparseRangeIterator;
  SparseRangeOffsetMap sparse_ranges_;
  bool sparse_file_open_ = false;

  // Offset of the end of the sparse file (where the next sparse range will be
  // written).
  int64_t sparse_tail_offset_;
};

}  // namespace disk_cache

#endif  // NET_DISK_CACHE_SIMPLE_SIMPLE_SYNCHRONOUS_ENTRY_H_
