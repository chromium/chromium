// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/renderer_client.h"

namespace media {

bool RendererClient::IsVideoStreamAvailable() {
  return true;
}

}  // namespace media
