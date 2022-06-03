// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_INPUT_FILE_PARSERS_H_
#define NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_INPUT_FILE_PARSERS_H_

#include "base/strings/string_piece.h"
#include "net/tools/transport_security_state_generator/transport_security_state_entry.h"

namespace net {

namespace transport_security_state {

class Pinsets;

// Extracts SPKI information from the preloaded pins file. The SPKI's can be
// in the form of a PEM certificate, a PEM public key, or a BASE64 string.
//
// More info on the format can be found in
// net/http/transport_security_state_static.pins
bool ParseCertificatesFile(base::StringPiece certs_input, Pinsets* pinsets);

// Parses the |json| string; copies the items under the "entries" key to
// |entries| and the pinsets under the "pinsets" key to |pinsets|.
//
// More info on the format can be found in
// net/http/transport_security_state_static.json
bool ParseJSON(base::StringPiece json,
               TransportSecurityStateEntries* entries,
               Pinsets* pinsets);

}  // namespace transport_security_state

}  // namespace net

#endif  // NET_TOOLS_TRANSPORT_SECURITY_STATE_GENERATOR_INPUT_FILE_PARSERS_H_
