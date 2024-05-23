// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_AV1_BUILDER_H_
#define MEDIA_GPU_AV1_BUILDER_H_

#include <optional>
#include <vector>

#include "media/gpu/media_gpu_export.h"
#include "third_party/libgav1/src/src/utils/constants.h"
#include "third_party/libgav1/src/src/utils/types.h"

namespace media {

// Helper class for AV1 to write packed bitstream data.
class MEDIA_GPU_EXPORT AV1BitstreamBuilder {
 public:
  AV1BitstreamBuilder();
  ~AV1BitstreamBuilder();
  AV1BitstreamBuilder(AV1BitstreamBuilder&&);

  void Write(uint64_t val, int num_bits);
  void WriteBool(bool val);
  // Spec 5.3.2.
  void WriteOBUHeader(libgav1::ObuType type,
                      bool extension_flag,
                      bool has_size);
  // Writes a value encoded in LEB128. Spec 4.10.5.
  void WriteValueInLeb128(uint32_t value,
                          std::optional<int> fixed_size = std::nullopt);
  std::vector<uint8_t> Flush() &&;
  size_t OutstandingBits() const { return total_outstanding_bits_; }
  // Writes bits for the byte alignment. Spec 5.3.5.
  void PutAlignBits();
  // Writes trailing bits. Spec 5.3.4.
  void PutTrailingBits();
  void AppendBitstreamBuffer(AV1BitstreamBuilder buffer);

 private:
  std::vector<std::pair<uint64_t, int>> queued_writes_;
  size_t total_outstanding_bits_ = 0;
};

}  // namespace media

#endif  // MEDIA_GPU_AV1_BUILDER_H_
