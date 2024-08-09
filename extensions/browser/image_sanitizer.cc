// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "extensions/browser/image_sanitizer.h"

#include "base/debug/dump_without_crashing.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "extensions/browser/extension_file_task_runner.h"
#include "extensions/common/extension_resource_path_normalizer.h"
#include "services/data_decoder/public/cpp/decode_image.h"
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
  bool delete_success = base::DeleteFile(path);
  return std::make_tuple(std::move(contents), read_success, delete_success);
}

std::pair<bool, std::vector<unsigned char>> EncodeImage(const SkBitmap& image) {
  std::vector<unsigned char> image_data;
  bool success = gfx::PNGCodec::EncodeBGRASkBitmap(
      image,
      /*discard_transparency=*/false, &image_data);
  return std::make_pair(success, std::move(image_data));
}

bool WriteFile(const base::FilePath& path,
               const std::vector<unsigned char>& data) {
  return base::WriteFile(path, data);
}

}  // namespace

// static
std::unique_ptr<ImageSanitizer> ImageSanitizer::CreateAndStart(
    scoped_refptr<Client> client,
    const base::FilePath& image_dir,
    const std::set<base::FilePath>& image_paths,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner) {
  std::unique_ptr<ImageSanitizer> sanitizer(
      new ImageSanitizer(client, image_dir, image_paths, io_task_runner));
  sanitizer->Start();
  return sanitizer;
}

ImageSanitizer::ImageSanitizer(
    scoped_refptr<Client> client,
    const base::FilePath& image_dir,
    const std::set<base::FilePath>& image_relative_paths,
    const scoped_refptr<base::SequencedTaskRunner>& io_task_runner)
    : image_dir_(image_dir),
      image_paths_(image_relative_paths),
      client_(std::move(client)),
      io_task_runner_(io_task_runner) {
  DCHECK(client_);
}

ImageSanitizer::~ImageSanitizer() = default;
ImageSanitizer::Client::~Client() = default;

void ImageSanitizer::Start() {
  if (image_paths_.empty()) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(&ImageSanitizer::ReportSuccess,
                                  weak_factory_.GetWeakPtr()));
    return;
  }

  std::set<base::FilePath> normalized_image_paths;
  for (const base::FilePath& path : image_paths_) {
    // Normalize paths as |image_paths_| can contain duplicates like "icon.png"
    // and "./icon.png" to avoid unpacking the same image twice.
    base::FilePath normalized_path;
    if (path.IsAbsolute() || path.ReferencesParent() ||
        !NormalizeExtensionResourcePath(path, &normalized_path)) {
      // Report the error asynchronously so the caller stack has chance to
      // unwind.
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
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
    io_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&ReadAndDeleteBinaryFile, full_image_path),
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
  data_decoder::DecodeImage(
      client_->GetDataDecoder(), image_data,
      data_decoder::mojom::ImageCodec::kDefault,
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
  if (decoded_image.colorType() != kN32_SkColorType) {
    // The renderer should be sending us N32 32bpp bitmaps in reply, otherwise
    // this can lead to out-of-bounds mistakes when transferring the pixels out
    // of the bitmap into other buffers.
    base::debug::DumpWithoutCrashing();
    ReportError(Status::kDecodingError, image_path);
    return;
  }

  // TODO(mpcomplete): It's lame that we're encoding all images as PNG, even
  // though they may originally be .jpg, etc.  Figure something out.
  // http://code.google.com/p/chromium/issues/detail?id=12459
  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&EncodeImage, decoded_image),
      base::BindOnce(&ImageSanitizer::ImageReencoded,
                     weak_factory_.GetWeakPtr(), image_path));

  client_->OnImageDecoded(image_path, decoded_image);
  // Note that the `client` callback could potentially delete `this` object.
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

  io_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteFile, image_dir_.Append(image_path),
                     std::move(image_data)),
      base::BindOnce(&ImageSanitizer::ImageWritten, weak_factory_.GetWeakPtr(),
                     image_path));
}

void ImageSanitizer::ImageWritten(const base::FilePath& image_path,
                                  bool success) {
  if (!success) {
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
  // Reset `client_` early, before the callback potentially deletes `this`.
  scoped_refptr<Client> client = std::move(client_);
  DCHECK(!client_);

  // The `client_` callback is the last statement, because it can potentially
  // delete `this` object.
  client->OnImageSanitizationDone(Status::kSuccess, base::FilePath());
}

void ImageSanitizer::ReportError(Status status, const base::FilePath& path) {
  // Prevent any other task from reporting, we want to notify only once.
  weak_factory_.InvalidateWeakPtrs();

  // Reset `client_` early, before the callback potentially deletes `this`.
  scoped_refptr<Client> client = std::move(client_);
  DCHECK(!client_);

  // The `client_` callback is the last statement, because it can potentially
  // delete `this` object.
  client->OnImageSanitizationDone(status, path);
}

}  // namespace extensions
