// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_CAST_PLATFORM_BINDINGS_IDS_H_
#define FUCHSIA_RUNNERS_CAST_CAST_PLATFORM_BINDINGS_IDS_H_

// Managed the space of unique identifiers for injected scripts, to prevent ID
// conflicts inside Cast Runner.
enum class CastPlatformBindingsId : uint64_t {
  NAMED_MESSAGE_PORT_CONNECTOR,
  CAST_CHANNEL,
  QUERYABLE_DATA,
  QUERYABLE_DATA_VALUES,
  NOT_IMPLEMENTED_API,
  TOUCH_INPUT,
};

#endif  // FUCHSIA_RUNNERS_CAST_CAST_PLATFORM_BINDINGS_IDS_H_
