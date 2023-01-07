// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/testing_api_binder.h"

#include "base/no_destructor.h"

namespace audio {

TestingApiBinder& GetTestingApiBinder() {
  static base::NoDestructor<TestingApiBinder> binder;
  return *binder;
}

SystemInfoBinder& GetSystemInfoBinderForTesting() {
  static base::NoDestructor<SystemInfoBinder> binder;
  return *binder;
}

}  // namespace audio
