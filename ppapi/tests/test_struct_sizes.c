/* Copyright 2010 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This test ensures (at compile time) that some types have the expected size in
 * C.  The purpose is to ensure that the ABI of PPAPI is known, consistent, and
 * stable.  Only structs that have architecture-dependent size are checked by
 * this test.  These structs use at least one type which differs in size between
 * 64-bit and 32-bit (e.g. pointers or long).  By convention, we require other
 * types to be of consistent size on 32-bit and 64-bit architectures.
 */

#include "ppapi/tests/all_c_includes.h"

#if !defined(__native_client__) && (defined(_M_X64) || defined(__x86_64__) || defined(__aarch64__) || defined(__mips64))
/* This section is for 64-bit compilation on Windows, Mac, and Linux.  Native
   client follows ILP32 even if -m64 is used, so NaCl code is explicitly treated
   as 32-bit.  This means pointers are always 4 bytes in native client, and it
   matches Win32  (see below).
 */
#include "ppapi/tests/arch_dependent_sizes_64.h"
#else
/* This section is for compilation on 32-bit targets plus native client (in both
   32-bit and 64-bit mode, since native client always conforms to ILP32).
 */
#include "ppapi/tests/arch_dependent_sizes_32.h"
#endif

