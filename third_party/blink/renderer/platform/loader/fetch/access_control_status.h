// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_ACCESS_CONTROL_STATUS_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_ACCESS_CONTROL_STATUS_H_

namespace blink {

enum AccessControlStatus {
  kSharableCrossOrigin,
  kOpaqueResource
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_ACCESS_CONTROL_STATUS_H_
