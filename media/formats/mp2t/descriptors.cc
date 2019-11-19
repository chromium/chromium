// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/formats/mp2t/descriptors.h"

#include <vector>

#include "base/logging.h"
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

const int kCASystemIdCenc = 0x6365;  // 'ce'
const uint32_t kFourccCbcs = 0x63626373;  // 'cbcs'

class StringBitReader : public BitReader {
 public:
  StringBitReader(const std::string& input);
  ~StringBitReader() override;
};

StringBitReader::StringBitReader(const std::string& input)
    : BitReader(reinterpret_cast<const uint8_t*>(input.data()), input.size()) {}

StringBitReader::~StringBitReader() {}

}  // namespace

Descriptors::Descriptors() {}

Descriptors::Descriptors(const Descriptors& other) = default;

Descriptors::~Descriptors() {}

bool Descriptors::Read(BitReader* reader, int size) {
  DCHECK(reader);
  DCHECK(size >= 0);
  descriptors_.clear();
  if (size == 0)
    return true;
  int initial_bits_read = reader->bits_read();
  int bits_read = 0;
  int bits_available = reader->bits_available();
  int size_in_bits = 8 * size;
  if (size_in_bits > bits_available)
    return false;
  bits_available = size_in_bits;
  do {
    int tag;
    size_t length;
    RCHECK(reader->ReadBits(8, &tag));
    RCHECK(reader->ReadBits(8, &length));
    char data[256];
    for (size_t i = 0; i < length; i++) {
      RCHECK(reader->ReadBits(8, &data[i]));
    }
    descriptors_.insert(Descriptor(tag, std::string(data, length)));
    bits_read = reader->bits_read() - initial_bits_read;
  } while (bits_read < bits_available);
  return bits_read == bits_available;
}

bool Descriptors::HasRegistrationDescriptor(
    int64_t* format_identifier,
    std::string* additional_info) const {
  DCHECK(format_identifier);
  DCHECK(additional_info);
  auto search = descriptors_.find(DESCRIPTOR_TAG_REGISTRATION);
  if (search == descriptors_.end())
    return false;
  const std::string& data = search->second;
  StringBitReader reader(data);
  RCHECK(reader.ReadBits(32, format_identifier));
  size_t extra_bits = reader.bits_available();
  RCHECK(extra_bits % 8 == 0);
  RCHECK(extra_bits > 0);
  RCHECK(reader.ReadString(extra_bits, additional_info));
  return true;
}

bool Descriptors::HasCADescriptor(int* system_id,
                                  int* pid,
                                  std::string* private_data) const {
  DCHECK(system_id);
  DCHECK(pid);
  DCHECK(private_data);
  auto search = descriptors_.find(DESCRIPTOR_TAG_CA);
  if (search == descriptors_.end())
    return false;
  const std::string& data = search->second;
  StringBitReader reader(data);
  RCHECK(reader.ReadBits(16, system_id));
  RCHECK(reader.SkipBits(3));
  RCHECK(reader.ReadBits(13, pid));
  size_t extra_bits = reader.bits_available();
  RCHECK(extra_bits % 8 == 0);
  RCHECK(reader.ReadString(extra_bits, private_data));
  return true;
}

bool Descriptors::HasCADescriptorCenc(int* ca_pid,
                                      int* pssh_pid,
                                      EncryptionScheme* scheme) const {
  DCHECK(ca_pid);
  DCHECK(pssh_pid);
  int system_id;
  std::string private_data;
  if (!HasCADescriptor(&system_id, ca_pid, &private_data))
    return false;
  if (system_id != kCASystemIdCenc)
    return false;
  StringBitReader reader(private_data);
  uint32_t scheme_type;
  uint32_t scheme_version;
  int num_systems;
  int encryption_algorithm;
  char pssh_system_id[16];
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
  for (size_t i = 0; i < 16; i++) {
    RCHECK(reader.ReadBits(8, &pssh_system_id[i]));
  }
  RCHECK(reader.ReadBits(13, pssh_pid));
  // The pattern is actually set differently for audio and video, so OK not to
  // set it here. Important thing is to set the cipher mode.
  *scheme = EncryptionScheme::kCbcs;

  return true;
}

bool Descriptors::HasPrivateDataIndicator(int64_t value) const {
  int64_t private_data_indicator;
  auto search = descriptors_.find(DESCRIPTOR_TAG_PRIVATE_DATA_INDICATOR);
  if (search == descriptors_.end())
    return false;
  const std::string& data = search->second;
  StringBitReader reader(data);
  RCHECK(reader.ReadBits(32, &private_data_indicator));
  RCHECK(reader.bits_available() == 0);
  return private_data_indicator == value;
}

}  // namespace mp2t
}  // namespace media
