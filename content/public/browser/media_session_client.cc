// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_session_client.h"

#include "base/check.h"

namespace content {

namespace {
MediaSessionClient* g_client = nullptr;
}

MediaSessionClient::MediaSessionClient() {
  CHECK(!g_client);
  g_client = this;
}

MediaSessionClient::~MediaSessionClient() {
  g_client = nullptr;
}

// static
MediaSessionClient* MediaSessionClient::Get() {
  return g_client;
}

}  // namespace content
