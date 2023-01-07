// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/data_decoder/public/cpp/service_provider.h"

namespace data_decoder {

namespace {

ServiceProvider* g_provider = nullptr;

}  // namespace

// static
void ServiceProvider::Set(ServiceProvider* provider) {
  g_provider = provider;
}

// static
ServiceProvider* ServiceProvider::Get() {
  return g_provider;
}

}  // namespace data_decoder
