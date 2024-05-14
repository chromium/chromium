// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/storage/settings_namespace.h"

#include "base/notreached.h"

namespace extensions {

namespace settings_namespace {

namespace {
const char kLocalNamespace[] = "local";
const char kSyncNamespace[] = "sync";
const char kManagedNamespace[] = "managed";
}  // namespace

std::string ToString(Namespace settings_namespace) {
  switch (settings_namespace) {
    case LOCAL:
      return kLocalNamespace;
    case SYNC:
      return kSyncNamespace;
    case MANAGED:
      return kManagedNamespace;
    case INVALID:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return std::string();
}

}  // namespace settings_namespace

}  // namespace extensions
