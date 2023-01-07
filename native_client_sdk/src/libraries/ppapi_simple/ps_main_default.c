/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

/* Default main entry point for ppapi_simple is the main() symbol */
#include "ppapi_simple/ps_main.h"

int main(int argc, char **argv);

PPAPI_SIMPLE_REGISTER_MAIN(main);
