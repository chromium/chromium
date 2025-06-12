// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/disk_cache/blockfile/file.h"

namespace disk_cache {

// Cross platform constructors. Platform specific code is in
// file_{win,posix}.cc.

File::File() : init_(false), mixed_(false) {}

File::File(bool mixed_mode) : init_(false), mixed_(mixed_mode) {}

}  // namespace disk_cache
