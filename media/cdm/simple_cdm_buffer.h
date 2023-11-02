// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SIMPLE_CDM_BUFFER_H_
#define MEDIA_CDM_SIMPLE_CDM_BUFFER_H_

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "media/cdm/api/content_decryption_module.h"

namespace media {

// cdm::Buffer implementation that provides access to memory. This is a simple
// implementation that stores the data in a std::vector<uint8_t>.
class SimpleCdmBuffer final : public cdm::Buffer {
 public:
  static SimpleCdmBuffer* Create(size_t capacity);

  SimpleCdmBuffer(const SimpleCdmBuffer&) = delete;
  SimpleCdmBuffer& operator=(const SimpleCdmBuffer&) = delete;

  // cdm::Buffer implementation.
  void Destroy() override;
  uint32_t Capacity() const override;
  uint8_t* Data() override;
  void SetSize(uint32_t size) override;
  uint32_t Size() const override;

 private:
  explicit SimpleCdmBuffer(uint32_t capacity);
  ~SimpleCdmBuffer() override;

  std::vector<uint8_t> buffer_;
  uint32_t size_;
};

}  // namespace media

#endif  // MEDIA_CDM_SIMPLE_CDM_BUFFER_H_
