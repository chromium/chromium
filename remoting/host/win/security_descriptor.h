// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_SECURITY_DESCRIPTOR_H_
#define REMOTING_HOST_WIN_SECURITY_DESCRIPTOR_H_

#include <windows.h>

#include <string>

#include "remoting/base/typed_buffer.h"

namespace remoting {

typedef TypedBuffer<ACL> ScopedAcl;
typedef TypedBuffer<SECURITY_DESCRIPTOR> ScopedSd;
typedef TypedBuffer<SID> ScopedSid;

// Converts an SDDL string into a binary self-relative security descriptor.
ScopedSd ConvertSddlToSd(const std::string& sddl);

// Converts a SID into a text string.
std::string ConvertSidToString(SID* sid);

// Returns the logon SID of a token. Returns nullptr if the token does not
// specify a logon SID or in case of an error.
ScopedSid GetLogonSid(HANDLE token);

// Converts a security descriptor in self-relative format to a security
// descriptor in absolute format.
bool MakeScopedAbsoluteSd(const ScopedSd& relative_sd,
                          ScopedSd* absolute_sd,
                          ScopedAcl* dacl,
                          ScopedSid* group,
                          ScopedSid* owner,
                          ScopedAcl* sacl);

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_SECURITY_DESCRIPTOR_H_
