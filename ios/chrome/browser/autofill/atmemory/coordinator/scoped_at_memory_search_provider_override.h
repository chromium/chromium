// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_SCOPED_AT_MEMORY_SEARCH_PROVIDER_OVERRIDE_H_
#define IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_SCOPED_AT_MEMORY_SEARCH_PROVIDER_OVERRIDE_H_

#import <Foundation/Foundation.h>

#import <memory>

@protocol AtMemorySearchProvider;

// C++ class that registers a mock AtMemorySearchProvider to override the
// search backend during tests. Only one override can be active at a time.
class ScopedAtMemorySearchProviderOverride {
 public:
  // Returns the currently active overridden search provider, if any.
  static id<AtMemorySearchProvider> Get();

  // Creates an instance that registers `provider` as the active override.
  // The override is removed when this object is destroyed.
  static std::unique_ptr<ScopedAtMemorySearchProviderOverride>
  MakeAndArmForTesting(id<AtMemorySearchProvider> provider);

  ScopedAtMemorySearchProviderOverride(
      const ScopedAtMemorySearchProviderOverride&) = delete;
  ScopedAtMemorySearchProviderOverride& operator=(
      const ScopedAtMemorySearchProviderOverride&) = delete;

  ~ScopedAtMemorySearchProviderOverride();

 private:
  explicit ScopedAtMemorySearchProviderOverride(
      id<AtMemorySearchProvider> provider);

  id<AtMemorySearchProvider> provider_;
};

#endif  // IOS_CHROME_BROWSER_AUTOFILL_ATMEMORY_COORDINATOR_SCOPED_AT_MEMORY_SEARCH_PROVIDER_OVERRIDE_H_
