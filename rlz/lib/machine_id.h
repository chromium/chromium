// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RLZ_LIB_MACHINE_ID_H_
#define RLZ_LIB_MACHINE_ID_H_


#include <string>

namespace rlz_lib {

// Gets the unique ID for the machine used for RLZ tracking purposes. On
// Windows, this ID is derived from the Windows machine SID, and is the string
// representation of a 20 byte hash + 4 bytes volum id + a 1 byte checksum.
// Included in financial pings with events, unless explicitly forbidden by the
// calling application.
bool GetMachineId(std::string* machine_id);

// Retrieves a raw machine identifier string and a machine-specific
// 4 byte value. GetMachineId() will SHA1 |data|, append |more_data|, compute
// the Crc8 of that, and return a hex-encoded string of that data.
bool GetRawMachineId(std::u16string* data, int* more_data);

namespace testing {
bool GetMachineIdImpl(const std::u16string& sid_string,
                      int volume_id,
                      std::string* machine_id);
}  // namespace testing

}  // namespace rlz_lib

#endif  // RLZ_LIB_MACHINE_ID_H_
