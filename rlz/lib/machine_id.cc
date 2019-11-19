// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rlz/lib/machine_id.h"

#include <stddef.h>

#include "base/hash/sha1.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "rlz/lib/assert.h"
#include "rlz/lib/crc8.h"
#include "rlz/lib/string_utils.h"

namespace rlz_lib {

bool GetMachineId(std::string* machine_id) {
  if (!machine_id)
    return false;

#if defined(OS_CHROMEOS)

  // Generate a random machine Id each time this function is called.  This
  // prevents the RLZ server from correlating two RLZ pings from the same
  // Chrome OS device.
  //
  // The Id should be 50 characters long and begin with "nonce-".  Generate 23
  // cryptographically random bytes, then convert to a printable string using
  // 2 hex digits per byte for a string of length 46 characters. Truncate last
  // hex character for 45 characters.
  unsigned char bytes[23];
  std::string str_bytes;
  base::RandBytes(bytes, sizeof(bytes));
  rlz_lib::BytesToString(bytes, sizeof(bytes), &str_bytes);
  str_bytes.resize(45);
  machine_id->clear();
  base::StringAppendF(machine_id, "NONCE%s", str_bytes.c_str());
  DCHECK_EQ(50u, machine_id->length());
  return true;

#else

  static std::string calculated_id;
  static bool calculated = false;
  if (calculated) {
    *machine_id = calculated_id;
    return true;
  }

  base::string16 sid_string;
  int volume_id;
  if (!GetRawMachineId(&sid_string, &volume_id))
    return false;

  if (!testing::GetMachineIdImpl(sid_string, volume_id, machine_id))
    return false;

  calculated = true;
  calculated_id = *machine_id;
  return true;

#endif  // defined(OS_CHROMEOS)
}

namespace testing {

bool GetMachineIdImpl(const base::string16& sid_string,
                      int volume_id,
                      std::string* machine_id) {
  machine_id->clear();

  // The ID should be the SID hash + the Hard Drive SNo. + checksum byte.
  static const int kSizeWithoutChecksum = base::kSHA1Length + sizeof(int);
  std::basic_string<unsigned char> id_binary(kSizeWithoutChecksum + 1, 0);

  if (!sid_string.empty()) {
    // In order to be compatible with the old version of RLZ, the hash of the
    // SID must be done with all the original bytes from the unicode string.
    // However, the chromebase SHA1 hash function takes only an std::string as
    // input, so the unicode string needs to be converted to std::string
    // "as is".
    size_t byte_count = sid_string.size() * sizeof(base::string16::value_type);
    const char* buffer = reinterpret_cast<const char*>(sid_string.c_str());
    std::string sid_string_buffer(buffer, byte_count);

    // Note that digest can have embedded nulls.
    std::string digest(base::SHA1HashString(sid_string_buffer));
    VERIFY(digest.size() == base::kSHA1Length);
    std::copy(digest.begin(), digest.end(), id_binary.begin());
  }

  // Convert from int to binary (makes big-endian).
  for (size_t i = 0; i < sizeof(int); i++) {
    int shift_bits = 8 * (sizeof(int) - i - 1);
    id_binary[base::kSHA1Length + i] = static_cast<unsigned char>(
        (volume_id >> shift_bits) & 0xFF);
  }

  // Append the checksum byte.
  if (!sid_string.empty() || (0 != volume_id))
    rlz_lib::Crc8::Generate(id_binary.c_str(),
                            kSizeWithoutChecksum,
                            &id_binary[kSizeWithoutChecksum]);

  return rlz_lib::BytesToString(
      id_binary.c_str(), kSizeWithoutChecksum + 1, machine_id);
}

}  // namespace testing

}  // namespace rlz_lib
