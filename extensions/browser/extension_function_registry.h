// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_FUNCTION_REGISTRY_H_
#define EXTENSIONS_BROWSER_EXTENSION_FUNCTION_REGISTRY_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "extensions/browser/extension_function_histogram_value.h"

class ExtensionFunction;

// A factory function for creating new ExtensionFunction instances.
using ExtensionFunctionFactory = scoped_refptr<ExtensionFunction> (*)();

// Template for defining ExtensionFunctionFactory.
template <class T>
scoped_refptr<ExtensionFunction> NewExtensionFunction() {
  return base::MakeRefCounted<T>();
}

// Contains a list of all known extension functions and allows clients to
// create instances of them.
class ExtensionFunctionRegistry {
 public:
  struct FactoryEntry {
   public:
    FactoryEntry();
    constexpr FactoryEntry(
        ExtensionFunctionFactory factory,
        const char* function_name,
        extensions::functions::HistogramValue histogram_value)
        : factory_(factory),
          function_name_(function_name),
          histogram_value_(histogram_value) {}

    ExtensionFunctionFactory factory_ = nullptr;
    const char* function_name_ = nullptr;
    extensions::functions::HistogramValue histogram_value_ =
        extensions::functions::UNKNOWN;
  };
  using FactoryMap = std::map<std::string, FactoryEntry>;

  static ExtensionFunctionRegistry& GetInstance();
  ExtensionFunctionRegistry();
  ~ExtensionFunctionRegistry();

  // Allows overriding of specific functions for testing.  Functions must be
  // previously registered.  Returns true if successful.
  bool OverrideFunctionForTesting(const std::string& name,
                                  ExtensionFunctionFactory factory);

  // Factory method for the ExtensionFunction registered as 'name'.
  scoped_refptr<ExtensionFunction> NewFunction(const std::string& name);

  // Registers a new extension function. This will override any existing entry.
  void Register(const FactoryEntry& entry);
  template <class T>
  void RegisterFunction() {
    Register(FactoryEntry(&NewExtensionFunction<T>, T::function_name(),
                          T::histogram_value()));
  }

  const FactoryMap& GetFactoriesForTesting() const { return factories_; }

 private:
  FactoryMap factories_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionFunctionRegistry);
};

#endif  // EXTENSIONS_BROWSER_EXTENSION_FUNCTION_REGISTRY_H_
