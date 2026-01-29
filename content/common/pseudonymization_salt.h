// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_PSEUDONYMIZATION_SALT_H_
#define CONTENT_COMMON_PSEUDONYMIZATION_SALT_H_

#include <stdint.h>

#include "base/memory/read_only_shared_memory_region.h"
#include "build/blink_buildflags.h"
#include "content/common/content_export.h"

namespace base {
class CommandLine;
}

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
CONTENT_EXPORT uint32_t GetPseudonymizationSalt();

// In the browser process, this is called during initialization to set the
// browser process salt. It is then called in each child processes via an IPC
// from the browser process to synchronize the same pseudonymization salt across
// each process.
//
// This function is thread-safe - it can be called on any thread.
CONTENT_EXPORT void SetPseudonymizationSalt(uint32_t salt);

// Allow the salt to be reset to zero. This allows unit tests that might share
// the same process to function correctly, since the salt is process-wide.
CONTENT_EXPORT void ResetSaltForTesting();

// Returns true if the pseudonymization salt has been initialized.
CONTENT_EXPORT bool IsSaltInitialized();

#if BUILDFLAG(USE_BLINK)
// Returns a shared memory region containing the salt for passing to child
// processes at launch time. See https://crbug.com/40850085.
// The region is created lazily on first call and cached for subsequent calls.
// Must only be called after SetPseudonymizationSalt().
CONTENT_EXPORT const base::ReadOnlySharedMemoryRegion&
GetPseudonymizationSaltSharedMemoryRegion();

// Attempts to initialize the salt from shared memory passed via command line.
// This should be called early in child process startup, before any tracing.
// No-op if the command line switch is not present.
CONTENT_EXPORT void MaybeInitializePseudonymizationSaltFromSharedMemory(
    const base::CommandLine& command_line);

namespace internal {

// Initializes the salt from a shared memory region passed at launch time.
// Returns true on success. Exposed in internal namespace for testing.
CONTENT_EXPORT bool InitializeSaltFromSharedMemory(
    base::ReadOnlySharedMemoryRegion region);

}  // namespace internal
#endif  // BUILDFLAG(USE_BLINK)

}  // namespace content

#endif  // CONTENT_COMMON_PSEUDONYMIZATION_SALT_H_
