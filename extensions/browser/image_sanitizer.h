// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_IMAGE_SANITIZER_H_
#define EXTENSIONS_BROWSER_IMAGE_SANITIZER_H_

#include <cstdint>
#include <memory>
#include <set>
#include <tuple>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/data_decoder/public/mojom/image_decoder.mojom.h"

class SkBitmap;

namespace data_decoder {
class DataDecoder;
}

namespace extensions {

// This class takes potentially unsafe images and decodes them in a sandboxed
// process, then reencodes them so that they can later be safely used in the
// browser process.
class ImageSanitizer {
 public:
  enum class Status {
    kSuccess = 0,
    kImagePathError,
    kFileReadError,
    kFileDeleteError,
    kDecodingError,
    kEncodingError,
    kFileWriteError,
  };

  class Client : public base::RefCountedThreadSafe<Client> {
   public:
    // Asks the client for a DataDecoder.  Pushing the ownership of the
    // DataDecoder to Client implementations help ensure that the same decoder
    // can be reused across different decoding kinds (including non-image
    // decoding).
    virtual data_decoder::DataDecoder* GetDataDecoder() = 0;

    // Callback invoked exactly once - when the image sanitization is done. If
    // status is an error, |path| points to the file that caused the error.
    virtual void OnImageSanitizationDone(Status status,
                                         const base::FilePath& path) = 0;

    // Callback invoked on a background thread 0..N times (once per image from
    // `image_relative_paths`) whenever an image has been successfully decoded.
    virtual void OnImageDecoded(const base::FilePath& path, SkBitmap image) = 0;

   protected:
    friend class base::RefCountedThreadSafe<Client>;
    virtual ~Client();
  };

  // Creates an ImageSanitizer and starts the sanitization of the images in
  // |image_relative_paths|. These paths should be relative and not reference
  // their parent dir or an kImagePathError will be reported to |done_callback|.
  // These relative paths are resolved against |image_dir|.
  //
  // |client| provides the DataDecoder to use for image decoding.  |client|'s
  // OnImageDecoded and OnImageSanitizationDone methods will be called with
  // sanitization results (if the returned ImageSanitizer instance is deleted
  // then these callback methods are not called and the sanitization stops
  // promptly (some background tasks may still run)).
  static std::unique_ptr<ImageSanitizer> CreateAndStart(
      scoped_refptr<Client> client,
      const base::FilePath& image_dir,
      const std::set<base::FilePath>& image_relative_paths,
      const scoped_refptr<base::SequencedTaskRunner>& task_runner);

  ImageSanitizer(const ImageSanitizer&) = delete;
  ImageSanitizer& operator=(const ImageSanitizer&) = delete;

  ~ImageSanitizer();

 private:
  ImageSanitizer(
      scoped_refptr<Client> client,
      const base::FilePath& image_dir,
      const std::set<base::FilePath>& image_relative_paths,
      const scoped_refptr<base::SequencedTaskRunner>& io_task_runner);

  void Start();

  void ImageFileRead(
      const base::FilePath& image_path,
      std::tuple<std::vector<uint8_t>, bool, bool> read_and_delete_result);

  void ImageDecoded(const base::FilePath& image_path,
                    const SkBitmap& decoded_image);

  void ImageReencoded(const base::FilePath& image_path,
                      std::pair<bool, std::vector<unsigned char>> result);

  void ImageWritten(const base::FilePath& image_path, bool success);

  void ReportSuccess();
  void ReportError(Status status, const base::FilePath& path);

  void CleanUp();

  base::FilePath image_dir_;
  std::set<base::FilePath> image_paths_;
  scoped_refptr<Client> client_;
  scoped_refptr<base::SequencedTaskRunner> io_task_runner_;
  base::WeakPtrFactory<ImageSanitizer> weak_factory_{this};
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_IMAGE_SANITIZER_H_
