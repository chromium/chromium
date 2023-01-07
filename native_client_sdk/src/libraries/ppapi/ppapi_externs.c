/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "ppapi/c/ppp.h"

/* Add a global symbol to force the linker to generate a LIB. */
void _lib_ppapi_dummy_symbol() {}
