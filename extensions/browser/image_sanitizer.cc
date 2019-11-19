// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/image_sanitizer.h"

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/task_runner_util.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/extension_resource_path_normalizer.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "ui/gfx/codec/png_codec.h"

namespace extensions {

namespace {

// We don't expect icons and other extension's images to be big.
// We use this limit to prevent from opening too large images.
const int kMaxImageCanvas = 4096 * 4096;  // 16MB

// Reads the file in |path| and then deletes it.
// Returns a tuple containing: the file content, whether the read was
// successful, whether the delete was successful.
std::tuple<std::vector<uint8_t>, bool, bool> ReadAndDeleteBinaryFile(
    const base::FilePath& path) {
  std::vector<uint8_t> contents;
  bool read_success = false;
  int64_t file_size;
  if (base::GetFileSize(path, &file_size)) {
    contents.resize(file_size);
    read_success =
        base::ReadFile(path, reinterpret_cast<char*>(contents.data()),
                       file_size) == file_size;
  }
  bool delete_success = base::DeleteFile(path, /*recursive=*/false);
  return std::make_tuple(std::move(contents), read_success, delete_success);
}

std::pair<bool, std::vector<unsigned char>> EncodeImage(const SkBitmap& image) {
  std::vector<unsigned char> image_data;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(
      image,
      /*discard_transparency=*/false, &image_data);
  return std::make_pair(success, std::move(image_data));
}

int WriteFile(const base::FilePath& path,
              const std::vector<unsigned char>& data) {
  return base::WriteFile(path, reinterpret_cast<const char*>(data.data()),
                         base::checked_cast<int>(data.size()));
}

}  // namespace

// static
std::unique_ptr<ImageSanitizer> ImageSanitizer::CreateAndStart(
    data_decoder::DataDecoder* decoder,
    const base::FilePath& image_dir,
    const std::set<base::FilePath>& image_paths,
    ImageDecodedCallback image_decoded_callback,
    SanitizationDoneCallback done_callback) {
  std::unique_ptr<ImageSanitizer> sanitizer(new ImageSanitizer(
      image_dir, image_paths, std::move(image_decoded_callback),
      std::move(done_callback)));
  sanitizer->Start(decoder);
  return sanitizer;
}

ImageSanitizer::ImageSanitizer(
    const base::FilePath& image_dir,
    const std::set<base::FilePath>& image_relative_paths,
    ImageDecodedCallback image_decoded_callback,
    SanitizationDoneCallback done_callback)
    : image_dir_(image_dir),
      image_paths_(image_relative_paths),
      image_decoded_callback_(std::move(image_decoded_callback)),
      done_callback_(std::move(done_callback)) {}

ImageSanitizer::~ImageSanitizer() = default;

void ImageSanitizer::Start(data_decoder::DataDecoder* decoder) {
  if (image_paths_.empty()) {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&ImageSanitizer::ReportSuccess,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  decoder->GetService()->BindImageDecoder(
      image_decoder_.BindNewPipeAndPassReceiver());
  image_decoder_.set_disconnect_handler(
      base::BindOnce(&ImageSanitizer::ReportError, weak_factory_.GetWeakPtr(),
                     Status::kServiceError, base::FilePath()));

  std::set<base::FilePath> normalized_image_paths;
  for (const base::FilePath& path : image_paths_) {
    // Normalize paths as |image_paths_| can contain duplicates like "icon.png"
    // and "./icon.png" to avoid unpacking the same image twice.
    base::FilePath normalized_path;
    if (path.IsAbsolute() || path.ReferencesParent() ||
        !NormalizeExtensionResourcePath(path, &normalized_path)) {
      // Report the error asynchronously so the caller stack has chance to
      // unwind.
      base::SequencedTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&ImageSanitizer::ReportError,
                                    weak_factory_.GetWeakPtr(),
                                    Status::kImagePathError, path));
      return;
    }
    normalized_image_paths.insert(normalized_path);
  }
  // Update |image_paths_| as some of the path might have been changed by
  // normalization.
  image_paths_ = std::move(normalized_image_paths);

  // Note that we use 2 for loops instead of one to prevent a race and flakyness
  // in tests: if |image_paths_| contains 2 paths, a valid one that points to a
  // file that does not exist and an invalid one, there is a race that can cause
  // either error to be reported (kImagePathError or kFileReadError).
  for (const base::FilePath& path : image_paths_) {
    base::FilePath full_image_path = image_dir_.Append(path);
    base::PostTaskAndReplyWithResult(
        extensions::GetExtensionFileTaskRunner().get(), FROM_HERE,
        base::BindOnce(&ReadAndDeleteBinaryFile, full_image_path),
        base::BindOnce(&ImageSanitizer::ImageFileRead,
                       weak_factory_.GetWeakPtr(), path));
  }
}

void ImageSanitizer::ImageFileRead(
    const base::FilePath& image_path,
    std::tuple<std::vector<uint8_t>, bool, bool> read_and_delete_result) {
  if (!std::get<1>(read_and_delete_result)) {
    ReportError(Status::kFileReadError, image_path);
    return;
  }
  if (!std::get<2>(read_and_delete_result)) {
    ReportError(Status::kFileDeleteError, image_path);
    return;
  }
  const std::vector<uint8_t>& image_data = std::get<0>(read_and_delete_result);
  image_decoder_->DecodeImage(
      image_data, data_decoder::mojom::ImageCodec::DEFAULT,
      /*shrink_to_fit=*/false, kMaxImageCanvas, gfx::Size(),
      base::BindOnce(&ImageSanitizer::ImageDecoded, weak_factory_.GetWeakPtr(),
                     image_path));
}

void ImageSanitizer::ImageDecoded(const base::FilePath& image_path,
                                  const SkBitmap& decoded_image) {
  if (decoded_image.isNull()) {
    ReportError(Status::kDecodingError, image_path);
    return;
  }

  if (image_decoded_callback_)
    image_decoded_callback_.Run(image_path, decoded_image);

  // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
  // though they may originally be .jpg, etc.  Figure something out.
  // http://code.google.com/p/chromium/issues/detail?id=12459
  base::PostTaskAndReplyWithResult(
      extensions::GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&EncodeImage, decoded_image),
      base::BindOnce(&ImageSanitizer::ImageReencoded,
                     weak_factory_.GetWeakPtr(), image_path));
}

void ImageSanitizer::ImageReencoded(
    const base::FilePath& image_path,
    std::pair<bool, std::vector<unsigned char>> result) {
  bool success = result.first;
  std::vector<unsigned char> image_data = std::move(result.second);
  if (!success) {
    ReportError(Status::kEncodingError, image_path);
    return;
  }

  int size = base::checked_cast<int>(image_data.size());
  base::PostTaskAndReplyWithResult(
      extensions::GetExtensionFileTaskRunner().get(), FROM_HERE,
      base::BindOnce(&WriteFile, image_dir_.Append(image_path),
                     std::move(image_data)),
      base::BindOnce(&ImageSanitizer::ImageWritten, weak_factory_.GetWeakPtr(),
                     image_path, size));
}

void ImageSanitizer::ImageWritten(const base::FilePath& image_path,
                                  int expected_size,
                                  int actual_size) {
  if (expected_size != actual_size) {
    ReportError(Status::kFileWriteError, image_path);
    return;
  }
  // We have finished with this path.
  size_t removed_count = image_paths_.erase(image_path);
  DCHECK_EQ(1U, removed_count);

  if (image_paths_.empty()) {
    // This was the last path, we are done.
    ReportSuccess();
  }
}

void ImageSanitizer::ReportSuccess() {
  CleanUp();
  std::move(done_callback_).Run(Status::kSuccess, base::FilePath());
}

void ImageSanitizer::ReportError(Status status, const base::FilePath& path) {
  CleanUp();
  // Prevent any other task from reporting, we want to notify only once.
  weak_factory_.InvalidateWeakPtrs();
  std::move(done_callback_).Run(status, path);
}

void ImageSanitizer::CleanUp() {
  image_decoder_.reset();
  // It's important to clear the repeating callback as it may cause a circular
  // reference (the callback holds a ref to an object that has a ref to |this|)
  // that would cause a leak.
  image_decoded_callback_.Reset();
}

}  // namespace extensions
