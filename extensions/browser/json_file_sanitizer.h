// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_JSON_FILE_SANITIZER_H_
#define EXTENSIONS_BROWSER_JSON_FILE_SANITIZER_H_

#include <memory>
#include <set>
#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/values.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/json_parser.mojom.h"

namespace data_decoder {
class DataDecoder;
}

namespace extensions {

// This class takes potentially unsafe JSON files, decodes them in a sandboxed
// process, then reencodes them so that they can later be parsed safely from the
// browser process.
// Note that at this time it this is limited to JSON files that contain a
// unique dictionary as their root and will fail with a kDecodingError if that
// is not the case.
class JsonFileSanitizer {
 public:
  enum class Status {
    kSuccess = 0,
    kFileReadError,
    kFileDeleteError,
    kDecodingError,
    kSerializingError,
    kFileWriteError,
  };

  // Callback invoked when the JSON sanitization is is done. If status is an
  // error, |error_msg| contains the error message.
  using Callback =
      base::OnceCallback<void(Status status, const std::string& error_msg)>;

  // Creates a JsonFileSanitizer and starts the sanitization of the JSON files
  // in |file_paths|.
  // |decoder| should be a DataDecoder which can be used to talk to a Data
  // Decoder service instance. It must be live on the calling sequence and
  // it is not retained beyond the extent of this call.
  // |callback| is invoked asynchronously when all JSON files have been
  // sanitized or if an error occurred.
  // If the returned JsonFileSanitizer instance is deleted before |callback| was
  // invoked, then |callback| is never invoked and the sanitization stops
  // promptly (some background tasks may still run).
  static std::unique_ptr<JsonFileSanitizer> CreateAndStart(
      data_decoder::DataDecoder* decoder,
      const std::set<base::FilePath>& file_paths,
      Callback callback);

  ~JsonFileSanitizer();

 private:
  JsonFileSanitizer(const std::set<base::FilePath>& file_paths,
                    Callback callback);

  void Start(data_decoder::DataDecoder* decoder);

  void JsonFileRead(const base::FilePath& file_path,
                    std::tuple<std::string, bool, bool> read_and_delete_result);

  void JsonParsingDone(const base::FilePath& file_path,
                       base::Optional<base::Value> json_value,
                       const base::Optional<std::string>& error);

  void JsonFileWritten(const base::FilePath& file_path,
                       int expected_size,
                       int actual_size);

  void ReportSuccess();

  void ReportError(Status status, const std::string& path);

  std::set<base::FilePath> file_paths_;
  Callback callback_;
  mojo::Remote<data_decoder::mojom::JsonParser> json_parser_;
  base::WeakPtrFactory<JsonFileSanitizer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(JsonFileSanitizer);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_JSON_FILE_SANITIZER_H_
