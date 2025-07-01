// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file defines empty versions of the macros used in the interfaces_*.h
// files, as long as they aren't already defined. This allows users of those
// files to only implement the macros they need, and everything else will
// compile.
//
// Include this file at the top, and interfaces_postamble.h at the bottom. The
// postamble will clean up these definitions.

#ifndef PROXIED_API
#define PROXIED_API(api_name)
#define UNDEFINE_PROXIED_API
#endif

#ifndef PROXIED_IFACE
#define PROXIED_IFACE(iface_str, iface_struct)
#define UNDEFINE_PROXIED_IFACE
#endif
