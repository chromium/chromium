// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_JSON_FILE_SANITIZER_H_
#define EXTENSIONS_BROWSER_JSON_FILE_SANITIZER_H_

#include <memory>
#include <optional>
#include <set>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace extensions {

// This class takes potentially unsafe JSON files, decodes them in a sandboxed
// process, then reencodes them so that they can later be parsed safely from the
// browser process.
// Note that at this time it this is limited to JSON files that contain a
// unique dictionary as their root and will fail with a kDecodingError if that
// is not the case.
class JsonFileSanitizer {
 public:
  enum class Error {
    kFileReadError,
    kFileDeleteError,
    kDecodingError,
    kSerializingError,
    kFileWriteError,
  };

  // Callback invoked when the JSON sanitization is is done.
  using Callback = base::OnceCallback<void(base::expected<void, Error>)>;

  // Creates a JsonFileSanitizer and starts the sanitization of the JSON files
  // in `file_paths`.
  // `callback` is invoked asynchronously when all JSON files have been
  // sanitized or if an error occurred.
  // If the returned JsonFileSanitizer instance is deleted before `callback` was
  // invoked, then `callback` is never invoked and the sanitization stops
  // promptly (some background tasks may still run).
  static std::unique_ptr<JsonFileSanitizer> CreateAndStart(
      const std::set<base::FilePath>& file_paths,
      Callback callback,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

  JsonFileSanitizer(const JsonFileSanitizer&) = delete;
  JsonFileSanitizer& operator=(const JsonFileSanitizer&) = delete;

  ~JsonFileSanitizer();

 private:
  JsonFileSanitizer(
      Callback callback,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

  void Start(const std::set<base::FilePath>& file_paths);

  // Note: unlike all other methods, this executes on `io_task_runner_`.
  static base::expected<void, Error> ProcessFile(const base::FilePath& path);

  void OnProcessedFile(base::expected<void, Error> result);
  void ReportSuccess();
  void ReportError(Error error);

  size_t remaining_callbacks_ = 0;
  Callback callback_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  base::WeakPtrFactory<JsonFileSanitizer> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_JSON_FILE_SANITIZER_H_
