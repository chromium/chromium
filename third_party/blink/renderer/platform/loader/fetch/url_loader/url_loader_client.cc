// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/fetch/url_loader/url_loader_client.h"

// This url_loader_client.cc, which includes only url_loader_client.h, because
// URLLoaderClient is not compiled without this cc file.
// So if we don't have this cc file, we will see unresolved symbol error when
// constructor/destructor's address is required.
