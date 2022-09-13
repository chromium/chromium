// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_ACTIVATION_SEQUENCE_H_
#define EXTENSIONS_COMMON_ACTIVATION_SEQUENCE_H_

#include "base/types/strong_alias.h"

namespace extensions {

// Unique identifier for an extension's activation->deactivation span.
//
// Applies to Service Worker based extensions. This is used to detect if a
// PendingTask for an extension expired at the time of executing the task, due
// to extension activation after deactivation.
using ActivationSequence =
    ::base::StrongAlias<class ActivationSequenceTag, int>;

}  // namespace extensions

#endif  // EXTENSIONS_COMMON_ACTIVATION_SEQUENCE_H_
