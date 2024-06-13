// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_LOG_FILE_NET_LOG_OBSERVER_H_
#define NET_LOG_FILE_NET_LOG_OBSERVER_H_

#include <limits>
#include <memory>
#include <optional>

#include "base/files/file.h"
#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/values.h"
#include "net/base/net_export.h"
#include "net/log/net_log.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace net {

// FileNetLogObserver watches the NetLog event stream and sends all entries to
// a file.
//
// Consumers must call StartObserving before calling StopObserving, and must
// call each method exactly once in the lifetime of the observer.
//
// The log will not be completely written until StopObserving is called.
//
// When a file size limit is given, FileNetLogObserver will create temporary
// directory containing chunks of events. This is used to drop older events in
// favor of newer ones.
class NET_EXPORT FileNetLogObserver : public NetLog::ThreadSafeObserver {
 public:
  // Special value meaning "can use an unlimited number of bytes".
  static constexpr uint64_t kNoLimit = std::numeric_limits<uint64_t>::max();

  // Creates an instance of FileNetLogObserver that writes observed netlog
  // events to |log_path|.
  //
  // |log_path| is where the final log file will be written to. If a file
  // already exists at this path it will be overwritten. While logging is in
  // progress, events may be written to a like-named directory.
  //
  // |max_total_size| is the limit on how many bytes logging may consume on
  // disk. This is an approximate limit, and in practice FileNetLogObserver may
  // (slightly) exceed it. This may be set to kNoLimit to remove any size
  // restrictions.
  //
  // |constants| is an optional legend for decoding constant values used in the
  // log. It should generally be a modified version of GetNetConstants(). If not
  // present, the output of GetNetConstants() will be used.
  // TODO(crbug.com/40257546): This should be updated to pass a
  // base::Value::Dict instead of a std::unique_ptr.
  static std::unique_ptr<FileNetLogObserver> CreateBounded(
      const base::FilePath& log_path,
      uint64_t max_total_size,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

  // Shortcut for calling CreateBounded() with kNoLimit.
  static std::unique_ptr<FileNetLogObserver> CreateUnbounded(
      const base::FilePath& log_path,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

  // Creates a bounded log that writes to a pre-existing file (truncating
  // it to start with, and closing it upon completion).  |inprogress_dir_path|
  // will be used as a scratch directory, for temporary files (with predictable
  // names).
  static std::unique_ptr<FileNetLogObserver> CreateBoundedPreExisting(
      const base::FilePath& inprogress_dir_path,
      base::File output_file,
      uint64_t max_total_size,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

  // Creates an unbounded log that writes to a pre-existing file (truncating
  // it to start with, and closing it upon completion).
  static std::unique_ptr<FileNetLogObserver> CreateUnboundedPreExisting(
      base::File output_file,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

  // Creates a bounded log that writes to a pre-existing. Instead of stitching
  // multiple log files together, once the maximum capacity has been reached the
  // logging stops.
  static std::unique_ptr<FileNetLogObserver> CreateBoundedFile(
      base::File output_file,
      uint64_t max_total_size,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

  FileNetLogObserver(const FileNetLogObserver&) = delete;
  FileNetLogObserver& operator=(const FileNetLogObserver&) = delete;

  ~FileNetLogObserver() override;

  // Attaches this observer to |net_log| and begins observing events.
  void StartObserving(NetLog* net_log);

  // Stops observing net_log() and closes the output file(s). Must be called
  // after StartObserving. Should be called before destruction of the
  // FileNetLogObserver and the NetLog, or the NetLog files (except for an
  // externally provided output_file) will be deleted when the observer is
  // destroyed. Note that it is OK to destroy |this| immediately after calling
  // StopObserving() - the callback will still be called once the file writing
  // has completed.
  //
  // |polled_data| is an optional argument used to add additional network stack
  // state to the log.
  //
  // If non-null, |optional_callback| will be run on whichever thread
  // StopObserving() was called on once all file writing is complete and the
  // netlog files can be accessed safely.
  void StopObserving(std::unique_ptr<base::Value> polled_data,
                     base::OnceClosure optional_callback);

  // NetLog::ThreadSafeObserver
  void OnAddEntry(const NetLogEntry& entry) override;

  // Same as CreateBounded() but you can additionally specify
  // |total_num_event_files|.
  static std::unique_ptr<FileNetLogObserver> CreateBoundedForTests(
      const base::FilePath& log_path,
      uint64_t max_total_size,
      size_t total_num_event_files,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

 private:
  class WriteQueue;
  class FileWriter;

  static std::unique_ptr<FileNetLogObserver> CreateInternal(
      const base::FilePath& log_path,
      const base::FilePath& inprogress_dir_path,
      std::optional<base::File> pre_existing_out_file,
      uint64_t max_total_size,
      size_t total_num_event_files,
      NetLogCaptureMode capture_mode,
      std::unique_ptr<base::Value::Dict> constants);

  FileNetLogObserver(scoped_refptr<base::SequencedTaskRunner> file_task_runner,
                     std::unique_ptr<FileWriter> file_writer,
                     scoped_refptr<WriteQueue> write_queue,
                     NetLogCaptureMode capture_mode,
                     std::unique_ptr<base::Value::Dict> constants);

  static std::string CaptureModeToString(NetLogCaptureMode mode);

  scoped_refptr<base::SequencedTaskRunner> file_task_runner_;

  // The |write_queue_| object is shared between the file task runner and the
  // main thread, and should be alive for the entirety of the observer's
  // lifetime. It should be destroyed once both the observer has been destroyed
  // and all tasks posted to the file task runner have completed.
  scoped_refptr<WriteQueue> write_queue_;

  // The FileNetLogObserver is shared between the main thread and
  // |file_task_runner_|.
  //
  // Conceptually FileNetLogObserver owns it, however on destruction its
  // deletion is deferred until outstanding tasks on |file_task_runner_| have
  // finished (since it is posted using base::Unretained()).
  std::unique_ptr<FileWriter> file_writer_;

  const NetLogCaptureMode capture_mode_;
};

// Serializes |value| to a JSON string used when writing to a file.
NET_EXPORT_PRIVATE std::string SerializeNetLogValueToJson(
    const base::ValueView& value);

}  // namespace net

#endif  // NET_LOG_FILE_NET_LOG_OBSERVER_H_
