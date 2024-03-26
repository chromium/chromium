// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_INPUT_FILE_PARSERS_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_INPUT_FILE_PARSERS_H_

#include <string_view>

#include "net/tools/transport_security_state_generator/transport_security_state_entry.h"

namespace base {
class Time;
}

namespace net::transport_security_state {

class Pinsets;

// Extracts SPKI information from the preloaded pins file. The SPKI's can be
// in the form of a PEM certificate, a PEM public key, or a BASE64 string.
//
// More info on the format can be found in
// net/http/transport_security_state_static.pins
bool ParseCertificatesFile(std::string_view certs_input,
                           Pinsets* pinsets,
                           base::Time* timestamp);

// Parses the |hsts_json| and |pins_json| strings; copies the items under the
// "entries" key to |entries| and the pinsets under the "pinsets" key to
// |pinsets|.
//
// More info on the format can be found in
// net/http/transport_security_state_static.json
bool ParseJSON(std::string_view hsts_json,
               std::string_view pins_json,
               TransportSecurityStateEntries* entries,
               Pinsets* pinsets);

}  // namespace net::transport_security_state

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_INPUT_FILE_PARSERS_H_
