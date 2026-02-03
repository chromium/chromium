// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/enterprise/encryption/encrypted_cache_file.h"

#include <algorithm>
#include <utility>

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "crypto/process_bound_string.h"

namespace network::enterprise_encryption {

namespace {

void RecordOpenResult(EncryptionError error) {
  base::UmaHistogramEnumeration("Enterprise.EncryptedCache.Open.Result", error);
}

void RecordReadResult(EncryptionError error) {
  base::UmaHistogramEnumeration("Enterprise.EncryptedCache.Read.Result", error);
}

void RecordWriteResult(EncryptionError error) {
  base::UmaHistogramEnumeration("Enterprise.EncryptedCache.Write.Result",
                                error);
}

int64_t GetPhysicalOffset(uint32_t chunk_index) {
  return kHeaderSize + static_cast<int64_t>(chunk_index) * kEncryptedChunkSize;
}

// Note: This limits the maximum file size to ~16TB (`kChunkDataSize` *
// UINT32_MAX), for now well beyond practical limits of the disk cache.
uint32_t GetChunkIndex(int64_t offset) {
  return offset / kChunkDataSize;
}

int64_t GetLogicalChunkStart(uint32_t chunk_index) {
  return static_cast<int64_t>(chunk_index) * kChunkDataSize;
}

}  // namespace

EncryptedCacheFile::EncryptedCacheFile(
    std::unique_ptr<disk_cache::CacheFile> file,
    const crypto::ProcessBoundString& primary_key)
    : file_(std::move(file)), key_(primary_key) {}

EncryptedCacheFile::~EncryptedCacheFile() = default;

bool EncryptedCacheFile::IsValid() const {
  return file_->IsValid();
}

base::File::Error EncryptedCacheFile::error_details() const {
  return file_->error_details();
}

std::optional<size_t> EncryptedCacheFile::Read(int64_t offset,
                                               base::span<uint8_t> data) {
  if (!EnsureInitialized()) {
    return std::nullopt;
  }

  if (data.empty()) {
    return 0;
  }

  uint32_t start_chunk_index = GetChunkIndex(offset);
  uint32_t end_chunk_index = GetChunkIndex(offset + data.size() - 1);
  size_t bytes_read = 0;

  // Decrypt all chunks intersecting with the read range.
  for (uint32_t chunk_index = start_chunk_index; chunk_index <= end_chunk_index;
       ++chunk_index) {
    auto result = ReadAndDecryptChunk(chunk_index);
    if (!result.has_value()) {
      RecordReadResult(EncryptionError::kDecryptionFailed);
      return std::nullopt;
    }
    const std::vector<uint8_t>& plaintext = result.value();

    int64_t chunk_start = GetLogicalChunkStart(chunk_index);
    int64_t chunk_end = chunk_start + plaintext.size();

    int64_t read_start = std::max(offset, chunk_start);
    int64_t read_end =
        std::min(offset + static_cast<int64_t>(data.size()), chunk_end);

    if (read_start < read_end) {
      // Copy the necessary slice of the chunk to the output buffer.
      size_t copy_size = read_end - read_start;
      size_t dest_offset = read_start - offset;
      size_t src_offset = read_start - chunk_start;

      auto dest_span = data.subspan(dest_offset, copy_size);
      dest_span.copy_from(base::span(plaintext).subspan(src_offset, copy_size));
      bytes_read += copy_size;
    } else {
      // If we overlap with the chunk conceptually but the chunk has no data in
      // that range (can happen at EOF), stop.
      break;
    }
  }
  RecordReadResult(EncryptionError::kSuccess);
  return bytes_read;
}

std::optional<size_t> EncryptedCacheFile::Write(
    int64_t offset,
    base::span<const uint8_t> data) {
  if (!EnsureInitialized()) {
    return std::nullopt;
  }

  if (data.empty()) {
    return 0;
  }

  int64_t current_logical_length = GetLength();
  int64_t new_logical_length = std::max(
      current_logical_length, offset + static_cast<int64_t>(data.size()));

  uint32_t start_chunk_index = GetChunkIndex(offset);
  uint32_t end_chunk_index = GetChunkIndex(offset + data.size() - 1);

  // Determine if we are extending the file and need to update the "last chunk"
  // flag.
  uint32_t old_last_chunk_index = 0;
  if (current_logical_length > 0) {
    old_last_chunk_index = GetChunkIndex(current_logical_length - 1);
  }

  // Handle separate writes (gaps) correctly by filling them with encrypted
  // zeros.
  if (offset > current_logical_length) {
    if (!SetLength(offset)) {
      return std::nullopt;
    }
    // Update length after extension.
    current_logical_length = offset;
    old_last_chunk_index = 0;
    if (current_logical_length > 0) {
      old_last_chunk_index = GetChunkIndex(current_logical_length - 1);
    }
  }

  // If write starts after the old last chunk, we need to pad/re-encrypt the old
  // last chunk to the full chunk size.
  if (current_logical_length > 0 && start_chunk_index > old_last_chunk_index) {
    if (!EnsurePreviousChunkNotLast(new_logical_length)) {
      return std::nullopt;
    }
  }

  uint32_t new_last_chunk_index = GetChunkIndex(new_logical_length - 1);

  size_t bytes_written = 0;

  // Write all chunks intersecting with the write range.
  for (uint32_t chunk_index = start_chunk_index; chunk_index <= end_chunk_index;
       ++chunk_index) {
    // Add flag if last chunk of the file to prevent truncation attacks.
    bool is_last_chunk = (chunk_index == new_last_chunk_index);

    // Calculate range intersection and slice input data for the bytes for this
    // specific chunk.
    int64_t chunk_start = GetLogicalChunkStart(chunk_index);
    int64_t write_start = std::max(offset, chunk_start);
    int64_t write_end =
        std::min(offset + static_cast<int64_t>(data.size()),
                 chunk_start + static_cast<int64_t>(kChunkDataSize));

    size_t chunk_write_size = write_end - write_start;
    size_t chunk_write_offset = write_start - chunk_start;
    auto data_to_write = data.subspan(bytes_written, chunk_write_size);

    bool is_new_chunk =
        (current_logical_length == 0) || (chunk_index > old_last_chunk_index);

    if (!WriteChunk(chunk_index, chunk_write_offset, data_to_write,
                    is_new_chunk, is_last_chunk)) {
      return std::nullopt;
    }

    bytes_written += chunk_write_size;
  }

  RecordWriteResult(EncryptionError::kSuccess);
  return bytes_written;
}

bool EncryptedCacheFile::GetInfo(base::File::Info* file_info) {
  if (!file_->GetInfo(file_info)) {
    return false;
  }
  file_info->size = GetLength();
  return true;
}

int64_t EncryptedCacheFile::GetLength() {
  if (!EnsureInitialized()) {
    return 0;
  }
  int64_t file_length = file_->GetLength();
  if (file_length <= static_cast<int64_t>(kHeaderSize)) {
    return 0;
  }

  // Calculate raw size of encrypted content without the header.
  int64_t content_length = file_length - kHeaderSize;
  int64_t full_chunks = content_length / kEncryptedChunkSize;
  int64_t remainder = content_length % kEncryptedChunkSize;

  if (remainder == 0) {
    return full_chunks * kChunkDataSize;
  }

  if (remainder < static_cast<int64_t>(kAuthTagSize)) {
    // A valid partial chunk must have at least an auth tag. If not, we return
    // the valid length up to the last chunk.
    return full_chunks * kChunkDataSize;
  }

  // Return logical size.
  return full_chunks * kChunkDataSize +
         (remainder - static_cast<int64_t>(kAuthTagSize));
}

bool EncryptedCacheFile::SetLength(int64_t length) {
  if (length < 0) {
    return false;
  }
  if (!EnsureInitialized()) {
    return false;
  }

  int64_t current_len = GetLength();

  if (length == current_len) {
    return true;
  }

  if (length == 0) {
    // No chunks to re-encrypt, just truncate to header size.
    return file_->SetLength(kHeaderSize);
  }

  // Truncation case.
  if (length < current_len) {
    uint32_t new_last_chunk_index = GetChunkIndex(length - 1);
    size_t len_in_chunk = length - GetLogicalChunkStart(new_last_chunk_index);

    // Read existing data from this chunk to preserve it, and resize it to the
    // new length.
    auto result = ReadAndDecryptChunk(new_last_chunk_index);
    if (!result.has_value()) {
      return false;
    }
    std::vector<uint8_t> plaintext = std::move(result.value());
    plaintext.resize(len_in_chunk, 0);

    // To avoid partial-update overhead in `WriteChunk`, we inline the
    // encryption here.
    std::vector<uint8_t> ciphertext = encryptor_->EncryptChunk(
        plaintext, new_last_chunk_index, /*is_last_chunk=*/true);
    int64_t offset = GetPhysicalOffset(new_last_chunk_index);
    if (!file_->WriteAndCheck(offset, ciphertext)) {
      return false;
    }
    int64_t new_phys_len = offset + plaintext.size() + kAuthTagSize;
    return file_->SetLength(new_phys_len);
  }

  // Extension case.
  // 32KB buffer size used to batch writes of encrypted zeros. This is for
  // optimization purposes only, to minimize allocation sizes for large
  // extensions.
  const int64_t kMaxPaddingChunkSize = 32 * 1024;

  // Create a buffer of zeros. We can't rely on the OS to pad with zeros because
  // they are not valid ciphertext in our scheme. To maintain integrity, we need
  // to encrypt the zeros.
  std::vector<uint8_t> zeros(
      std::min(length - current_len, kMaxPaddingChunkSize), 0);

  while (current_len < length) {
    size_t write_size =
        std::min(length - current_len, static_cast<int64_t>(zeros.size()));
    if (write_size != zeros.size()) {
      zeros.resize(write_size);
    }

    auto val = Write(current_len, base::span(zeros));
    if (!val.has_value()) {
      return false;
    }

    current_len += write_size;
  }

  return true;
}

bool EncryptedCacheFile::ReadAndCheck(int64_t offset,
                                      base::span<uint8_t> data) {
  auto res = Read(offset, data);
  return res.has_value() && res.value() == data.size();
}

bool EncryptedCacheFile::WriteAndCheck(int64_t offset,
                                       base::span<const uint8_t> data) {
  auto res = Write(offset, data);
  return res.has_value() && res.value() == data.size();
}

bool EncryptedCacheFile::EnsureInitialized() {
  if (initialized_) {
    return true;
  }

  int64_t file_length = file_->GetLength();
  if (file_length < 0) {
    return false;
  }

  if (file_length == 0) {
    // New file: Create and write header.
    auto result = CreateHeader(base::as_byte_span(key_.secure_value()));
    if (!result.has_value()) {
      RecordOpenResult(EncryptionError::kInvalidKey);
      return false;
    }
    auto& [header, context] = result.value();
    if (!file_->WriteAndCheck(0, header)) {
      return false;
    }
    encryptor_ = std::make_unique<ChunkedEncryptor>(context);
  } else {
    // Existing file: Read and parse header.
    if (file_length < static_cast<int64_t>(kHeaderSize)) {
      RecordOpenResult(EncryptionError::kInvalidHeader);
      return false;
    }

    std::vector<uint8_t> header_bytes(kHeaderSize);
    if (!file_->ReadAndCheck(0, header_bytes)) {
      return false;
    }

    auto context_or_error =
        ParseHeader(header_bytes, base::as_byte_span(key_.secure_value()));
    if (!context_or_error.has_value()) {
      RecordOpenResult(context_or_error.error());
      return false;
    }
    encryptor_ =
        std::make_unique<ChunkedEncryptor>(std::move(context_or_error.value()));
  }

  RecordOpenResult(EncryptionError::kSuccess);
  initialized_ = true;
  return true;
}

bool EncryptedCacheFile::WriteChunk(uint32_t chunk_index,
                                    size_t offset_in_chunk,
                                    base::span<const uint8_t> data_to_write,
                                    bool is_new_chunk,
                                    bool is_last_chunk) {
  size_t chunk_write_size = data_to_write.size();
  std::vector<uint8_t> plaintext_buf;
  base::span<const uint8_t> plaintext_span;

  // Optimization, no need to read existing data for full overwrites.
  bool full_overwrite =
      (offset_in_chunk == 0 && chunk_write_size == kChunkDataSize);

  if (full_overwrite) {
    plaintext_span = data_to_write;
  } else {
    // Partial update.
    if (is_new_chunk) {
      // New chunk partial write: Pad unwritten areas with zeros so we encrypt
      // defined data.
      size_t new_size = offset_in_chunk + chunk_write_size;
      plaintext_buf.resize(new_size);
      base::span(plaintext_buf)
          .subspan(offset_in_chunk, chunk_write_size)
          .copy_from(data_to_write);
    } else {
      // Existing chunk partial write: Read-Modify-Write.
      auto read_result = ReadAndDecryptChunk(chunk_index);
      if (!read_result.has_value()) {
        return false;
      }
      plaintext_buf = std::move(read_result.value());
      // Extend if we write past end.
      if (offset_in_chunk + chunk_write_size > plaintext_buf.size()) {
        plaintext_buf.resize(offset_in_chunk + chunk_write_size);
      }
      base::span(plaintext_buf)
          .subspan(offset_in_chunk, chunk_write_size)
          .copy_from(data_to_write);
    }
    plaintext_span = plaintext_buf;
  }

  std::vector<uint8_t> ciphertext =
      encryptor_->EncryptChunk(plaintext_span, chunk_index, is_last_chunk);
  int64_t offset = GetPhysicalOffset(chunk_index);
  return file_->WriteAndCheck(offset, ciphertext);
}

base::expected<std::vector<uint8_t>, EncryptionError>
EncryptedCacheFile::ReadAndDecryptChunk(uint32_t chunk_index) {
  int64_t file_length = file_->GetLength();
  int64_t chunk_offset = GetPhysicalOffset(chunk_index);

  if (chunk_offset >= file_length) {
    // Reading past EOF.
    return base::unexpected(EncryptionError::kDecryptionFailed);
  }

  size_t to_read = kEncryptedChunkSize;
  bool is_last_chunk = false;

  if (chunk_offset + static_cast<int64_t>(to_read) >= file_length) {
    to_read = file_length - chunk_offset;
    is_last_chunk = true;
  }

  std::vector<uint8_t> ciphertext(to_read);
  if (!file_->ReadAndCheck(chunk_offset, ciphertext)) {
    return base::unexpected(EncryptionError::kDecryptionFailed);
  }

  auto decrypt_result =
      encryptor_->DecryptChunk(ciphertext, chunk_index, is_last_chunk);
  return decrypt_result;
}

bool EncryptedCacheFile::EnsurePreviousChunkNotLast(
    int64_t new_logical_length) {
  int64_t current_len = GetLength();
  if (current_len == 0) {
    return true;
  }

  uint32_t old_last_chunk_index = GetChunkIndex(current_len - 1);
  uint32_t new_last_chunk_index = 0;
  if (new_logical_length > 0) {
    new_last_chunk_index = GetChunkIndex(new_logical_length - 1);
  }

  // If we are extending beyond the old last chunk, that old chunk is no longer
  // last. We must re-encrypt it.
  if (new_logical_length > current_len &&
      new_last_chunk_index > old_last_chunk_index) {
    auto result = ReadAndDecryptChunk(old_last_chunk_index);
    if (!result.has_value()) {
      RecordReadResult(EncryptionError::kDecryptionFailed);
      return false;
    }
    std::vector<uint8_t> data = std::move(result.value());
    // Pad to full chunk size since it is no longer the last chunk.
    if (data.size() < kChunkDataSize) {
      data.resize(kChunkDataSize, 0);
    }

    // Re-encrypt without the last chunk flag.
    // Use offset 0 and full data size to trigger full overwrite optimization in
    // `WriteChunk`.
    if (!WriteChunk(old_last_chunk_index, 0, data, /*is_new_chunk=*/false,
                    /*is_last_chunk=*/false)) {
      return false;
    }
  }

  return true;
}

}  // namespace network::enterprise_encryption
