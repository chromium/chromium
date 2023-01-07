/* Copyright 2013 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#include "xray/xray_priv.h"

/* Note name demangling requires linking against libstdc++                 */
/* If your platform does not support __cxa_demangle, re-compile XRay with: */
/*   -DXRAY_NO_DEMANGLE                                                    */

#if !defined(XRAY_NO_DEMANGLE)
extern
char* __cxa_demangle(const char* __mangled_name, char* __output_buffer,
                     size_t* __length, int* __status);
#endif

const char* XRayDemangle(char* demangle, size_t size, const char* symbol) {
#if !defined(XRAY_NO_DEMANGLE)
  int stat;
  __cxa_demangle(symbol, demangle, &size, &stat);
  if (stat == 0)
    return demangle;
#endif
  return symbol;
}
