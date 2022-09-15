/* Copyright 2012 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * This file has compile assertions for the sizes of types that are dependent
 * on the architecture for which they are compiled (i.e., 32-bit vs 64-bit).
 */

#ifndef PPAPI_TESTS_ARCH_DEPENDENT_SIZES_64_H_
#define PPAPI_TESTS_ARCH_DEPENDENT_SIZES_64_H_

PP_COMPILE_ASSERT_SIZE_IN_BYTES(GLintptr, 8);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(GLsizeiptr, 8);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_CompletionCallback_Func, 8);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PP_URLLoaderTrusted_StatusCallback, 8);
PP_COMPILE_ASSERT_STRUCT_SIZE_IN_BYTES(PP_CompletionCallback, 24);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PPB_VideoDecoder_Dev, 64);
PP_COMPILE_ASSERT_SIZE_IN_BYTES(PPP_VideoDecoder_Dev, 32);

#endif  /* PPAPI_TESTS_ARCH_DEPENDENT_SIZES_64_H_ */
