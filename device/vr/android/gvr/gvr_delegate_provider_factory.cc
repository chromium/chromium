// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/vr/android/gvr/gvr_delegate_provider_factory.h"

#include "base/check_op.h"

namespace device {

namespace {
GvrDelegateProviderFactory* g_gvr_delegate_provider_factory = nullptr;
}  // namespace

// static
GvrDelegateProvider* GvrDelegateProviderFactory::Create() {
  if (!g_gvr_delegate_provider_factory)
    return nullptr;
  return g_gvr_delegate_provider_factory->CreateGvrDelegateProvider();
}

// static
void GvrDelegateProviderFactory::Install(
    std::unique_ptr<GvrDelegateProviderFactory> factory) {
  DCHECK_NE(g_gvr_delegate_provider_factory, factory.get());
  if (g_gvr_delegate_provider_factory)
    delete g_gvr_delegate_provider_factory;
  g_gvr_delegate_provider_factory = factory.release();
}

GvrDevice* GvrDelegateProviderFactory::device_ = nullptr;

}  // namespace device
