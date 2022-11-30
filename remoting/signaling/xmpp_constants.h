// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Contains constants for the IQ stanzas inherited by the signaling protocol
// from XMPP.

#ifndef REMOTING_SIGNALING_XMPP_CONSTANTS_H_
#define REMOTING_SIGNALING_XMPP_CONSTANTS_H_

#include "third_party/libjingle_xmpp/xmllite/qname.h"

namespace remoting {

extern const char kIqTypeSet[];
extern const char kIqTypeResult[];
extern const char kIqTypeError[];

extern const jingle_xmpp::StaticQName kQNameIq;
extern const jingle_xmpp::StaticQName kQNameId;
extern const jingle_xmpp::StaticQName kQNameType;
extern const jingle_xmpp::StaticQName kQNameTo;
extern const jingle_xmpp::StaticQName kQNameFrom;

}  // namespace remoting

#endif  // REMOTING_SIGNALING_XMPP_CONSTANTS_H_
