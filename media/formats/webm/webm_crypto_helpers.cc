// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/webm/webm_crypto_helpers.h"

#include <array>
#include <memory>

#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/logging.h"
#include "base/numerics/byte_conversions.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/decrypt_config.h"
#include "media/formats/webm/webm_constants.h"

namespace media {
namespace {

// Generates a 16 byte CTR counter block. The CTR counter block format is a
// CTR IV appended with a CTR block counter. |iv| is an 8 byte CTR IV.
// |iv_size| is the size of |iv| in bytes. Returns a string of
// kDecryptionKeySize bytes.
std::string GenerateWebMCounterBlock(base::span<const uint8_t> iv) {
  std::string counter_block(iv.begin(), iv.end());
  counter_block.append(DecryptConfig::kDecryptionKeySize - iv.size(), 0);
  return counter_block;
}

bool ExtractSubsamples(base::span<const uint8_t> buf,
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
  for (size_t i = 0; i <= num_partitions; ++i) {
    const uint32_t prev_partition_offset = partition_offset;
    if (i < num_partitions) {
      // For each partition, the offset is read from the partition offset data.
      partition_offset = base::U32FromBigEndian(
          buf.subspan(kWebMEncryptedFramePartitionOffsetSize * i)
              .first<kWebMEncryptedFramePartitionOffsetSize>());
    } else {
      // On the last iteration, we're past the last partition offset in `buf`,
      // and the offset is the remaining bytes in the frame.
      partition_offset = frame_data_size;
    }
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

bool WebMCreateDecryptConfig(const uint8_t* data_ptr,
                             int data_size,
                             const uint8_t* key_id_ptr,
                             int key_id_size,
                             std::unique_ptr<DecryptConfig>* decrypt_config,
                             int* data_offset) {
  // TODO(crbug.com/40284755):: The function should receive a span, not a
  // pointer/length pair.
  auto data =
      UNSAFE_TODO(base::span(data_ptr, base::checked_cast<size_t>(data_size)));
  // TODO(crbug.com/40284755):: The function should receive a span, not a
  // pointer/length pair.
  auto key_id = UNSAFE_TODO(
      base::span(key_id_ptr, base::checked_cast<size_t>(key_id_size)));
  auto reader = base::SpanReader(data);

  uint8_t signal_byte;
  static_assert(sizeof(signal_byte) == kWebMSignalByteSize);
  if (!reader.ReadU8BigEndian(signal_byte)) {
    DVLOG(1) << "Got a block from an encrypted stream with no data.";
    return false;
  }

  std::string counter_block;
  std::vector<SubsampleEntry> subsample_entries;

  if (signal_byte & kWebMFlagEncryptedFrame) {
    base::span<const uint8_t> iv;
    if (!reader.ReadInto(size_t{kWebMIvSize}, iv)) {
      DVLOG(1) << "Got an encrypted block with not enough data " << data.size();
      return false;
    }
    counter_block = GenerateWebMCounterBlock(iv);

    if (signal_byte & kWebMFlagEncryptedFramePartitioned) {
      uint8_t num_partitions;
      static_assert(sizeof(num_partitions) ==
                    kWebMEncryptedFrameNumPartitionsSize);
      if (!reader.ReadU8BigEndian(num_partitions)) {
        DVLOG(1) << "Got a partitioned encrypted block with not enough data "
                 << data.size();
        return false;
      }

      base::span<const uint8_t> partition_data;
      if (!reader.ReadInto(
              size_t{kWebMEncryptedFramePartitionOffsetSize} * num_partitions,
              partition_data) ||
          reader.remaining() == 0u) {
        DVLOG(1) << "Got a partitioned encrypted block with " << num_partitions
                 << " partitions but not enough data " << data.size();
        return false;
      }
      if (!ExtractSubsamples(partition_data, reader.remaining(), num_partitions,
                             &subsample_entries)) {
        return false;
      }
    }
  }

  if (counter_block.empty()) {
    // If the frame is unencrypted the DecryptConfig object should be NULL.
    decrypt_config->reset();
  } else {
    *decrypt_config = DecryptConfig::CreateCencConfig(
        std::string(key_id.begin(), key_id.end()), counter_block,
        subsample_entries);
  }
  *data_offset = base::checked_cast<int>(data.size() - reader.remaining());

  return true;
}

}  // namespace media
