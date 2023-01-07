// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_FORMATS_MP2T_DESCRIPTORS_H_
#define MEDIA_FORMATS_MP2T_DESCRIPTORS_H_

#include <stdint.h>

#include <map>
#include <string>

#include "media/base/decrypt_config.h"

namespace media {

class BitReader;

namespace mp2t {

// Representation of a list of descriptors, used in the MPEG-2 Systems standard
// to extend the definitions of programs or program elements. While the standard
// appears to permit multiple descriptors in such a list to have the same tag
// value, the implementation herein will not support this.
class Descriptors {
 public:
  Descriptors();
  Descriptors(const Descriptors& other);
  ~Descriptors();

  // Attempts to read a (possibly empty) list of descriptors from the |reader|.
  // If |size| > 0, the descriptors must occupy exactly |size| bytes, Otherwise,
  // the descriptors should use all available bits from the reader.
  bool Read(BitReader* reader, int size);

  // Indicates whether a Registration descriptor is present. If so, the
  // |format_identifier| and |additional_info| values are populated with the
  // contents of the descriptor.
  bool HasRegistrationDescriptor(int64_t* format_identifier,
                                 std::string* additional_info) const;

  // Indicates whether a CA descriptor is present. If so, the |system_id|,
  // |pid|, and |private_data| values are populated with the contents of the
  // descriptor.
  bool HasCADescriptor(int* system_id,
                       int* pid,
                       std::string* private_data) const;

  // Indicates whether a CA descriptor is present, and if so, whether it is
  // of the type defined by ISO/IEC 23001-9:2014 (i.e. with a specific
  // system_id value and layout of the private_data). If so, the |ca_pid|,
  // |pssh_pid| and |mode| are populated with the contents of the descriptor.
  bool HasCADescriptorCenc(int* ca_pid,
                           int* pssh_pid,
                           EncryptionScheme* scheme) const;

  // Indicates whether a Private Data Indicator descriptor is present with a
  // particular |value|.
  bool HasPrivateDataIndicator(int64_t value) const;

 private:
  using Descriptor = std::pair<int, std::string>;
  std::map<int, std::string> descriptors_;

  // Allow copy and assign so that it can be used in a std C++ container.
};

}  // namespace mp2t
}  // namespace media

#endif  // MEDIA_FORMATS_MP2T_DESCRIPTORS_H_
