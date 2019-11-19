// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/tools/gssapi/gss_types.h"

// These two imports follow the same pattern as those in gss_methods.cc but are
// separated out so that we can build a GSSAPI library that's missing a couple
// of imports.

extern "C" GSS_EXPORT OM_uint32
gss_import_name(OM_uint32* minor_status,
                const gss_buffer_t input_name_buffer,
                const gss_OID input_name_type,
                gss_name_t* output_name) {
  return 0;
}

extern "C" GSS_EXPORT OM_uint32 gss_release_name(OM_uint32* minor_status,
                                                 gss_name_t* input_name) {
  *minor_status = 0;
  delete *input_name;
  *input_name = nullptr;
  return 0;
}
