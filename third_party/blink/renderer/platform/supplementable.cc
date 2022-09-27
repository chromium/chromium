// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/supplementable.h"

// This Supplementable.cpp, which includes only
// Supplementable.h, should be in Source/platform,
// because Supplementable is not compiled without this cpp.
// So if we don't have this cpp, we will see unresolved symbol error
// when constructor/destructor's address is required.
// i.e. error LNK2005: "public: virtual __cdecl
// blink::SupplementTracing<0>::~SupplementTracing<0>(void)"
