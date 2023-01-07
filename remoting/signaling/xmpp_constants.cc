// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/signaling/xmpp_constants.h"

namespace remoting {

const char kIqTypeSet[] = "set";
const char kIqTypeResult[] = "result";
const char kIqTypeError[] = "error";

const jingle_xmpp::StaticQName kQNameIq = {"jabber:client", "iq"};
const jingle_xmpp::StaticQName kQNameId = {"", "id"};
const jingle_xmpp::StaticQName kQNameType = {"", "type"};
const jingle_xmpp::StaticQName kQNameTo = {"", "to"};
const jingle_xmpp::StaticQName kQNameFrom = {"", "from"};

}  // namespace remoting
