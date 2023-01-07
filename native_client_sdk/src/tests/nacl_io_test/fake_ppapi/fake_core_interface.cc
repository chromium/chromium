// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fake_ppapi/fake_core_interface.h"

FakeCoreInterface::FakeCoreInterface(FakeResourceManager* manager)
    : resource_manager_(manager) {}

void FakeCoreInterface::AddRefResource(PP_Resource handle) {
  return resource_manager_->AddRef(handle);
}

void FakeCoreInterface::ReleaseResource(PP_Resource handle) {
  return resource_manager_->Release(handle);
}
