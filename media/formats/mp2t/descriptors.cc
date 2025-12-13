// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/descriptors.h"

#include <array>
#include <vector>

#include "base/check.h"
#include "base/numerics/safe_conversions.h"
#include "media/base/bit_reader.h"
#include "media/base/encryption_pattern.h"
#include "media/formats/mp2t/mp2t_common.h"

namespace media {
namespace mp2t {

namespace {

// Tag values for various kinds of descriptors for which there is specific
// parsing support herein.
enum DescriptorTag {
  DESCRIPTOR_TAG_REGISTRATION = 5,
  DESCRIPTOR_TAG_CA = 9,
  DESCRIPTOR_TAG_PRIVATE_DATA_INDICATOR = 15,
};

const uint16_t kCASystemIdCenc = 0x6365;  // 'ce'
const uint32_t kFourccCbcs = 0x63626373;  // 'cbcs'

}  // namespace

Descriptors::Descriptors() = default;

Descriptors::Descriptors(const Descriptors& other) = default;

Descriptors::~Descriptors() {}

bool Descriptors::Read(BitReader* reader, int size) {
  DCHECK(reader);
  DCHECK(size >= 0);
  descriptors_.clear();
  if (size == 0) {
    return true;
  }
  size_t initial_bits_read = reader->bits_read();
  size_t bits_read = 0;
  size_t bits_available = reader->bits_available();
  size_t size_in_bits = 8 * base::checked_cast<size_t>(size);
  if (size_in_bits > bits_available) {
    return false;
  }
  bits_available = size_in_bits;
  do {
    uint8_t tag;
    size_t length;
    RCHECK(reader->ReadBits(8, &tag));
    RCHECK(reader->ReadBits(8, &length));
    std::vector<uint8_t> data(length);
    RCHECK(reader->ReadSpan(data));
    descriptors_.insert(Descriptor(tag, std::move(data)));
    bits_read = reader->bits_read() - initial_bits_read;
  } while (bits_read < bits_available);
  return bits_read == bits_available;
}

bool Descriptors::HasRegistrationDescriptor(
    uint32_t* format_identifier,
    std::vector<uint8_t>* additional_info) const {
  DCHECK(format_identifier);
  DCHECK(additional_info);
  auto search = descriptors_.find(DESCRIPTOR_TAG_REGISTRATION);
  if (search == descriptors_.end()) {
    return false;
  }
  const std::vector<unsigned char>& data = search->second;
  BitReader reader(data);
  RCHECK(reader.ReadBits(32, format_identifier));
  size_t extra_bits = reader.bits_available();
  RCHECK(extra_bits % 8 == 0);
  RCHECK(extra_bits > 0);
  additional_info->resize(extra_bits / 8);
  RCHECK(reader.ReadSpan(*additional_info));
  return true;
}

bool Descriptors::HasCADescriptor(uint16_t* system_id,
                                  uint16_t* pid,
                                  std::vector<uint8_t>* private_data) const {
  DCHECK(system_id);
  DCHECK(pid);
  DCHECK(private_data);
  auto search = descriptors_.find(DESCRIPTOR_TAG_CA);
  if (search == descriptors_.end()) {
    return false;
  }
  const std::vector<unsigned char>& data = search->second;
  BitReader reader(data);
  RCHECK(reader.ReadBits(16, system_id));
  RCHECK(reader.SkipBits(3));
  RCHECK(reader.ReadBits(13, pid));
  size_t extra_bits = reader.bits_available();
  if (extra_bits == 0) {
    return true;
  }
  RCHECK(extra_bits % 8 == 0);
  private_data->resize(extra_bits / 8);
  RCHECK(reader.ReadSpan(*private_data));
  return true;
}

bool Descriptors::HasCADescriptorCenc(uint16_t* ca_pid,
                                      uint16_t* pssh_pid,
                                      EncryptionScheme* scheme) const {
  DCHECK(ca_pid);
  DCHECK(pssh_pid);
  uint16_t system_id;
  std::vector<uint8_t> private_data;
  if (!HasCADescriptor(&system_id, ca_pid, &private_data)) {
    return false;
  }
  if (system_id != kCASystemIdCenc) {
    return false;
  }
  BitReader reader(private_data);
  uint32_t scheme_type;
  uint32_t scheme_version;
  uint8_t num_systems;
  uint32_t encryption_algorithm;
  std::array<uint8_t, 16> pssh_system_id;
  // TODO(dougsteed). Currently we don't check many of the following values,
  // and we only support the 'cbcs' scheme (which involves AES-CBC encryption).
  // When we flesh out this implementation to cover all of ISO/IEC 23001-9 we
  // will need to use and check these values more comprehensively.
  RCHECK(reader.ReadBits(32, &scheme_type));
  RCHECK(scheme_type == kFourccCbcs);
  RCHECK(reader.ReadBits(32, &scheme_version));
  RCHECK(reader.ReadBits(8, &num_systems));
  RCHECK(num_systems == 1);
  RCHECK(reader.ReadBits(24, &encryption_algorithm));
  RCHECK(reader.ReadSpan(pssh_system_id));
  RCHECK(reader.ReadBits(13, pssh_pid));
  // The pattern is actually set differently for audio and video, so OK not to
  // set it here. Important thing is to set the cipher mode.
  *scheme = EncryptionScheme::kCbcs;

  return true;
}

bool Descriptors::HasPrivateDataIndicator(uint32_t value) const {
  uint32_t private_data_indicator;
  auto search = descriptors_.find(DESCRIPTOR_TAG_PRIVATE_DATA_INDICATOR);
  if (search == descriptors_.end()) {
    return false;
  }
  const std::vector<uint8_t>& data = search->second;
  BitReader reader(data);
  RCHECK(reader.ReadBits(32, &private_data_indicator));
  RCHECK(reader.bits_available() == 0);
  return private_data_indicator == value;
}

}  // namespace mp2t
}  // namespace media
