// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/webapps_client.h"

namespace webapps {

namespace {
WebappsClient* g_instance = nullptr;
}

WebappsClient::WebappsClient() {
  DCHECK(!g_instance);
  g_instance = this;
}

WebappsClient::~WebappsClient() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

// static
WebappsClient* WebappsClient::Get() {
  return g_instance;
}

}  // namespace webapps
