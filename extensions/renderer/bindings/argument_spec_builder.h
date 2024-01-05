// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_BINDINGS_ARGUMENT_SPEC_BUILDER_H_
#define EXTENSIONS_RENDERER_BINDINGS_ARGUMENT_SPEC_BUILDER_H_

#include <memory>
#include <set>
#include <string_view>
#include <vector>

#include "extensions/renderer/bindings/argument_spec.h"

namespace extensions {

// A utility class for helping construct ArgumentSpecs in tests.
// NOTE: This is designed to be test-only. It's not worth adding to production
// code because it's a) only a bit of syntactic sugar and b) rife with footguns.
class ArgumentSpecBuilder {
 public:
  explicit ArgumentSpecBuilder(ArgumentType type);
  ArgumentSpecBuilder(ArgumentType type, std::string_view name);

  ArgumentSpecBuilder(const ArgumentSpecBuilder&) = delete;
  ArgumentSpecBuilder& operator=(const ArgumentSpecBuilder&) = delete;

  ~ArgumentSpecBuilder();

  ArgumentSpecBuilder& MakeOptional();
  ArgumentSpecBuilder& AddProperty(std::string_view property_name,
                                   std::unique_ptr<ArgumentSpec> property_spec);
  ArgumentSpecBuilder& SetMinimum(int minimum);
  ArgumentSpecBuilder& SetListType(std::unique_ptr<ArgumentSpec> list_type);
  ArgumentSpecBuilder& SetRef(std::string_view ref);
  ArgumentSpecBuilder& SetChoices(
      std::vector<std::unique_ptr<ArgumentSpec>> choices);
  ArgumentSpecBuilder& SetEnums(std::set<std::string> enum_values);
  ArgumentSpecBuilder& SetAdditionalProperties(
      std::unique_ptr<ArgumentSpec> additional_properties);
  ArgumentSpecBuilder& SetInstanceOf(std::string instance_of);
  ArgumentSpecBuilder& PreserveNull();
  std::unique_ptr<ArgumentSpec> Build();

 private:
  std::unique_ptr<ArgumentSpec> spec_;
  ArgumentSpec::PropertiesMap properties_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_BINDINGS_ARGUMENT_SPEC_BUILDER_H_
