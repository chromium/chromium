// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_FILE_READER_H_
#define EXTENSIONS_BROWSER_FILE_READER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/task/single_thread_task_runner.h"
#include "extensions/common/extension_resource.h"

// This file defines an interface for reading files asynchronously on a
// background sequence.
// Consider abstracting out a FilePathProvider (ExtensionResource) and moving
// back to chrome/browser/net if other subsystems want to use it.
class FileReader : public base::RefCountedThreadSafe<FileReader> {
 public:
  // Passes the result of loading the files in `data`, or reports the
  // encountered error in `error`. If there was an error, `data` will be empty.
  using DoneCallback =
      base::OnceCallback<void(std::vector<std::unique_ptr<std::string>> data,
                              std::optional<std::string> error)>;

  // Lets the caller accomplish tasks on the file data, after the file content
  // has been read. This is called once per file successfully read (it is not
  // invoked if a file read fails).
  using OptionalFileSequenceTask = base::RepeatingCallback<void(std::string*)>;

  FileReader(std::vector<extensions::ExtensionResource> resources,
             size_t max_resources_length,
             OptionalFileSequenceTask file_sequence_task,
             DoneCallback done_callback);

  FileReader(const FileReader&) = delete;
  FileReader& operator=(const FileReader&) = delete;

  // Called to start reading the files on a background sequence. Upon
  // completion, the callback will be notified of the results.
  void Start();

 private:
  friend class base::RefCountedThreadSafe<FileReader>;

  ~FileReader();

  void ReadFilesOnFileSequence();

  std::vector<extensions::ExtensionResource> resources_;
  const size_t max_resources_length_;
  OptionalFileSequenceTask optional_file_sequence_task_;
  DoneCallback done_callback_;
  const scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;
};

#endif  // EXTENSIONS_BROWSER_FILE_READER_H_
