// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EXPORT_IMPL_H_
#define NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EXPORT_IMPL_H_

#include "base/component_export.h"

// These macros are documented in:
// net/third_party/quiche/src/quiche/common/platform/api/quiche_export.h

#define QUICHE_EXPORT_IMPL COMPONENT_EXPORT(QUICHE)
#define QUICHE_NO_EXPORT_IMPL

#endif  // NET_THIRD_PARTY_QUICHE_OVERRIDES_QUICHE_PLATFORM_IMPL_QUICHE_EXPORT_IMPL_H_
