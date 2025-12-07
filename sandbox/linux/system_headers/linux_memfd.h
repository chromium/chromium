// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_MEMFD_H_
#define SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_MEMFD_H_

/* flags for memfd_create(2) (unsigned int) */

#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#ifndef MFD_ALLOW_SEALING
#define MFD_ALLOW_SEALING 0x0002U
#endif

#ifndef MFD_HUGETLB
#define MFD_HUGETLB 0x0004U
#endif

/* not executable and sealed to prevent changing to executable. */
#ifndef MFD_NOEXEC_SEAL
#define MFD_NOEXEC_SEAL 0x0008U
#endif

/* executable */
#ifndef MFD_EXEC
#define MFD_EXEC 0x0010U
#endif

#endif  // SANDBOX_LINUX_SYSTEM_HEADERS_LINUX_MEMFD_H_
