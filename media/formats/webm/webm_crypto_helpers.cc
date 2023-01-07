// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_crypto_helpers.h"

#include <memory>

#include "base/logging.h"
#include "base/sys_byteorder.h"
#include "media/base/decrypt_config.h"
#include "media/formats/webm/webm_constants.h"

namespace media {
namespace {

// Generates a 16 byte CTR counter block. The CTR counter block format is a
// CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
// |iv_size| is the size of |iv| in btyes. Returns a string of
// kDecryptionKeySize bytes.
std::string GenerateWebMCounterBlock(const uint8_t* iv, int iv_size) {
  std::string counter_block(reinterpret_cast<const char*>(iv), iv_size);
  counter_block.append(DecryptConfig::kDecryptionKeySize - iv_size, 0);
  return counter_block;
}

uint32_t ReadInteger(const uint8_t* buf, int size) {
  // Read in the big-endian integer.
  uint32_t value = 0;
  for (int i = 0; i < size; ++i)
    value = (value << 8) | buf[i];
  return value;
}

bool ExtractSubsamples(const uint8_t* buf,
                       size_t frame_data_size,
                       size_t num_partitions,
                       std::vector<SubsampleEntry>* subsample_entries) {
  subsample_entries->clear();
  uint32_t clear_bytes = 0;
  // Partition is the wall between alternating sections. Partition offsets are
  // relative to the start of the actual frame data.
  // Size of clear/cipher sections can be calculated from the difference between
  // adjacent partition offsets.
  // Here is an example with 4 partitions (5 sections):
  //   "clear |1 cipher |2 clear |3 cipher |4 clear"
  // With the first and the last implicit partition included:
  //   "|0 clear |1 cipher |2 clear |3 cipher |4 clear |5"
  //   where partition_offset_0 = 0, partition_offset_5 = frame_data_size
  // There are three subsamples in the above example:
  //   Subsample0.clear_bytes = partition_offset_1 - partition_offset_0
  //   Subsample0.cipher_bytes = partition_offset_2 - partition_offset_1
  //   ...
  //   Subsample2.clear_bytes = partition_offset_5 - partition_offset_4
  //   Subsample2.cipher_bytes = 0
  uint32_t partition_offset = 0;
  for (size_t i = 0, offset = 0; i <= num_partitions; ++i) {
    const uint32_t prev_partition_offset = partition_offset;
    partition_offset =
        (i == num_partitions)
            ? frame_data_size
            : ReadInteger(buf + offset, kWebMEncryptedFramePartitionOffsetSize);
    offset += kWebMEncryptedFramePartitionOffsetSize;
    if (partition_offset < prev_partition_offset) {
      DVLOG(1) << "Partition should not be decreasing " << prev_partition_offset
               << " " << partition_offset;
      return false;
    }

    uint32_t cipher_bytes = 0;
    bool new_subsample_entry = false;
    // Alternating clear and cipher sections.
    if ((i % 2) == 0) {
      clear_bytes = partition_offset - prev_partition_offset;
      // Generate a new subsample when finishing reading partition offsets.
      new_subsample_entry = i == num_partitions;
    } else {
      cipher_bytes = partition_offset - prev_partition_offset;
      // Generate a new subsample after seeing a cipher section.
      new_subsample_entry = true;
    }

    if (new_subsample_entry) {
      if (clear_bytes == 0 && cipher_bytes == 0) {
        DVLOG(1) << "Not expecting >2 partitions with the same offsets.";
        return false;
      }
      subsample_entries->push_back(SubsampleEntry(clear_bytes, cipher_bytes));
    }
  }
  return true;
}

}  // namespace anonymous

bool WebMCreateDecryptConfig(const uint8_t* data,
                             int data_size,
                             const uint8_t* key_id,
                             int key_id_size,
                             std::unique_ptr<DecryptConfig>* decrypt_config,
                             int* data_offset) {
  if (data_size < kWebMSignalByteSize) {
    DVLOG(1) << "Got a block from an encrypted stream with no data.";
    return false;
  }

  const uint8_t signal_byte = data[0];
  int frame_offset = sizeof(signal_byte);
  std::string counter_block;
  std::vector<SubsampleEntry> subsample_entries;

  if (signal_byte & kWebMFlagEncryptedFrame) {
    if (data_size < kWebMSignalByteSize + kWebMIvSize) {
      DVLOG(1) << "Got an encrypted block with not enough data " << data_size;
      return false;
    }
    counter_block = GenerateWebMCounterBlock(data + frame_offset, kWebMIvSize);
    frame_offset += kWebMIvSize;

    if (signal_byte & kWebMFlagEncryptedFramePartitioned) {
      if (data_size < frame_offset + kWebMEncryptedFrameNumPartitionsSize) {
        DVLOG(1) << "Got a partitioned encrypted block with not enough data "
                 << data_size;
        return false;
      }

      const size_t num_partitions = data[frame_offset];
      frame_offset += kWebMEncryptedFrameNumPartitionsSize;
      const uint8_t* partition_data_start = data + frame_offset;
      frame_offset += kWebMEncryptedFramePartitionOffsetSize * num_partitions;
      if (data_size <= frame_offset) {
        DVLOG(1) << "Got a partitioned encrypted block with " << num_partitions
                 << " partitions but not enough data " << data_size;
        return false;
      }
      const size_t frame_data_size = data_size - frame_offset;
      if (!ExtractSubsamples(partition_data_start, frame_data_size,
                             num_partitions, &subsample_entries)) {
        return false;
      }
    }
  }

  if (counter_block.empty()) {
    // If the frame is unencrypted the DecryptConfig object should be NULL.
    decrypt_config->reset();
  } else {
    *decrypt_config = DecryptConfig::CreateCencConfig(
        std::string(reinterpret_cast<const char*>(key_id), key_id_size),
        counter_block, subsample_entries);
  }
  *data_offset = frame_offset;

  return true;
}

}  // namespace media
