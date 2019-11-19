// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/values.h"
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
    kServiceError,  // The data-decoder service crashed.
  };

  // Callback invoked when the image sanitization is is done. If status is an
  // error, |path| points to the file that caused the error.
  using SanitizationDoneCallback =
      base::OnceCallback<void(Status status, const base::FilePath& path)>;

  // Called on a background thread when an image has been decoded.
  using ImageDecodedCallback =
      base::RepeatingCallback<void(const base::FilePath& path, SkBitmap image)>;

  // Creates an ImageSanitizer and starts the sanitization of the images in
  // |image_relative_paths|. These paths should be relative and not reference
  // their parent dir or an kImagePathError will be reported to |done_callback|.
  // These relative paths are resolved against |image_dir|.
  // |decoder| should be a DataDecoder instance to use for image decoding.
  // |done_callback| is invoked asynchronously when all images have been
  // sanitized or if an error occurred.
  // If the returned ImageSanitizer instance is deleted, |done_callback| and
  // |image_decoded_callback| are not called and the sanitization stops promptly
  // (some background tasks may still run).
  static std::unique_ptr<ImageSanitizer> CreateAndStart(
      data_decoder::DataDecoder* decoder,
      const base::FilePath& image_dir,
      const std::set<base::FilePath>& image_relative_paths,
      ImageDecodedCallback image_decoded_callback,
      SanitizationDoneCallback done_callback);

  ~ImageSanitizer();

 private:
  ImageSanitizer(const base::FilePath& image_dir,
                 const std::set<base::FilePath>& image_relative_paths,
                 ImageDecodedCallback image_decoded_callback,
                 SanitizationDoneCallback done_callback);

  void Start(data_decoder::DataDecoder* decoder);

  void ImageFileRead(
      const base::FilePath& image_path,
      std::tuple<std::vector<uint8_t>, bool, bool> read_and_delete_result);

  void ImageDecoded(const base::FilePath& image_path,
                    const SkBitmap& decoded_image);

  void ImageReencoded(const base::FilePath& image_path,
                      std::pair<bool, std::vector<unsigned char>> result);

  void ImageWritten(const base::FilePath& image_path,
                    int expected_size,
                    int actual_size);

  void ReportSuccess();
  void ReportError(Status status, const base::FilePath& path);

  void CleanUp();

  base::FilePath image_dir_;
  std::set<base::FilePath> image_paths_;
  ImageDecodedCallback image_decoded_callback_;
  SanitizationDoneCallback done_callback_;
  mojo::Remote<data_decoder::mojom::ImageDecoder> image_decoder_;
  base::WeakPtrFactory<ImageSanitizer> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ImageSanitizer);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_IMAGE_SANITIZER_H_
