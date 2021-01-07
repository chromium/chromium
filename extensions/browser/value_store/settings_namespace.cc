// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/value_store/settings_namespace.h"

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
  NOTREACHED();
  return std::string();
}

Namespace FromString(const std::string& namespace_string) {
  if (namespace_string == kLocalNamespace)
    return LOCAL;
  if (namespace_string == kSyncNamespace)
    return SYNC;
  if (namespace_string == kManagedNamespace)
    return MANAGED;
  return INVALID;
}

}  // namespace settings_namespace

}  // namespace extensions
