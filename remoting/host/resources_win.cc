// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resources.h"

namespace remoting {

bool LoadResources(const std::string& pref_locale) {
  // Do nothing since .pak files are not used on Windows.
  return false;
}

void UnloadResources() {}

}  // namespace remoting
