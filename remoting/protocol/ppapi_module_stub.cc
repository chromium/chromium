// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/module.h"

namespace pp {

// Factory function for your specialization of the Module object.
Module* CreateModule() {
  return nullptr;
}

}  // namespace pp
