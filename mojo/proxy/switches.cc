// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/proxy/switches.h"

namespace switches {

// The integer value of a file descriptor inherited by the mojo_proxy process
// when launched by its host. This descriptor references a Unix socket which is
// connected to the legacy client application to be the target of this proxy.
// Required.
const char kLegacyClientFd[] = "legacy-client-fd";

// The integer value of a file descriptor inherited by the mojo_proxy process
// when launched by its host. This descriptor references a Unix socket which is
// connected to the host process which launched this proxy to sit between the
// host and some legacy client application.
// Required.
const char kHostIpczTransportFd[] = "host-ipcz-transport-fd";

// By default, mojo_proxy assumes its host is a broker. When this flag is given
// it instead assumes its host is a non-broker who is offering to share their
// broker. The proxy must be configured correctly in this regard or all
// connections through it will fail.
const char kInheritIpczBroker[] = "inherit-ipcz-broker";

// For client applications who expect a single Mojo invitation attachment with a
// free-form name assigned to it, this specifies that attachment name. Either
// this or kNumericAttachmentNames must be specified on the command line.
const char kAttachmentName[] = "attachment-name";

// For client applications who expect Mojo invitation attachments to be assigned
// zero-based 64-bit integral values, this specifies the number of in-use
// attachments. The names are implicitly sequental integers starting from 0.
const char kNumAttachments[] = "num-attachments";

}  // namespace switches
