// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Cleans up after interfaces_preamble.h, see that file for more.

#ifdef UNDEFINE_PROXIED_API
#undef UNDEFINE_PROXIED_API
#undef PROXIED_API
#endif

#ifdef UNDEFINE_PROXIED_IFACE
#undef UNDEFINE_PROXIED_IFACE
#undef PROXIED_IFACE
#endif
