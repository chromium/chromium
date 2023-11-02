// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PSEUDONYMIZATION_SALT_H_
#define CONTENT_COMMON_PSEUDONYMIZATION_SALT_H_

#include <stdint.h>

namespace content {

// Gets the pseudonymization salt.
//
// Note that this function returns the same salt in all Chromium processes (e.g.
// in the Browser process, the Renderer processes and other child processes),
// because the propagation taking place via callers of SetPseudonymizationSalt
// below.  This behavior ensures that the
// content::PseudonymizationUtil::PseudonymizeString method produces the same
// results across all processes.
//
// This function is thread-safe - it can be called on any thread.
//
// PRIVACY NOTE: It is important that the returned value is never persisted
// anywhere or sent to a server.  Whoever has access to the salt can
// de-anonymize results of the content::PseudonymizationUtil::PseudonymizeString
// method.
uint32_t GetPseudonymizationSalt();

// Called in child processes, for setting the pseudonymization `salt` received
// in an IPC from a parent process.
//
// This function is thread-safe - it can be called on any thread.
void SetPseudonymizationSalt(uint32_t salt);

}  // namespace content

#endif  // CONTENT_COMMON_PSEUDONYMIZATION_SALT_H_
