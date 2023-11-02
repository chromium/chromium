// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_NACL_IRT_IRT_INTERFACES_H_
#define PPAPI_NACL_IRT_IRT_INTERFACES_H_

#include <stddef.h>
#include <stdlib.h>

extern const struct nacl_irt_ppapihook nacl_irt_ppapihook;
extern const struct nacl_irt_private_pnacl_translator_link
    nacl_irt_private_pnacl_translator_link;
extern const struct nacl_irt_private_pnacl_translator_compile
    nacl_irt_private_pnacl_translator_compile;

size_t chrome_irt_query(const char* interface_ident,
                        void* table, size_t tablesize);

#endif  // PPAPI_NACL_IRT_IRT_INTERFACES_H_
