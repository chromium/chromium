// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/atmemory/coordinator/scoped_at_memory_search_provider_override.h"

#import <ostream>

#import "base/check.h"
#import "base/check_op.h"
#import "base/memory/ptr_util.h"
#import "ios/chrome/browser/autofill/atmemory/public/at_memory_search_provider.h"

namespace {
// The active provider override.
__weak id<AtMemorySearchProvider> g_override_provider = nil;
}  // namespace

// static
id<AtMemorySearchProvider> ScopedAtMemorySearchProviderOverride::Get() {
  return g_override_provider;
}

// static
std::unique_ptr<ScopedAtMemorySearchProviderOverride>
ScopedAtMemorySearchProviderOverride::MakeAndArmForTesting(  // IN-TEST
    id<AtMemorySearchProvider> provider) {
  return base::WrapUnique(new ScopedAtMemorySearchProviderOverride(provider));
}

ScopedAtMemorySearchProviderOverride::ScopedAtMemorySearchProviderOverride(
    id<AtMemorySearchProvider> provider)
    : provider_(provider) {
  DCHECK(!g_override_provider)
      << "Only one ScopedAtMemorySearchProviderOverride can be active.";
  g_override_provider = provider_;
}

ScopedAtMemorySearchProviderOverride::~ScopedAtMemorySearchProviderOverride() {
  DCHECK_EQ(g_override_provider, provider_);
  g_override_provider = nil;
}
