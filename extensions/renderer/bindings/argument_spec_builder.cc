// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/bindings/argument_spec_builder.h"

#include <string_view>

namespace extensions {

ArgumentSpecBuilder::ArgumentSpecBuilder(ArgumentType type)
    : ArgumentSpecBuilder(type, std::string_view()) {}

ArgumentSpecBuilder::ArgumentSpecBuilder(ArgumentType type,
                                         std::string_view name)
    : spec_(std::make_unique<ArgumentSpec>(type)) {
  if (!name.empty())
    spec_->set_name(name);
}

ArgumentSpecBuilder::~ArgumentSpecBuilder() = default;

ArgumentSpecBuilder& ArgumentSpecBuilder::MakeOptional() {
  spec_->set_optional(true);
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::AddProperty(
    std::string_view property_name,
    std::unique_ptr<ArgumentSpec> property_spec) {
  properties_[std::string(property_name)] = std::move(property_spec);
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetMinimum(int minimum) {
  spec_->set_minimum(minimum);
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetListType(
    std::unique_ptr<ArgumentSpec> list_type) {
  spec_->set_list_element_type(std::move(list_type));
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetRef(std::string_view ref) {
  spec_->set_ref(ref);
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetChoices(
    std::vector<std::unique_ptr<ArgumentSpec>> choices) {
  spec_->set_choices(std::move(choices));
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetEnums(
    std::set<std::string> enum_values) {
  spec_->set_enum_values(std::move(enum_values));
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetAdditionalProperties(
    std::unique_ptr<ArgumentSpec> additional_properties) {
  spec_->set_additional_properties(std::move(additional_properties));
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::SetInstanceOf(
    std::string instance_of) {
  spec_->set_instance_of(std::move(instance_of));
  return *this;
}

ArgumentSpecBuilder& ArgumentSpecBuilder::PreserveNull() {
  spec_->set_preserve_null(true);
  return *this;
}

std::unique_ptr<ArgumentSpec> ArgumentSpecBuilder::Build() {
  spec_->set_properties(std::move(properties_));
  return std::move(spec_);
}

}  // namespace extensions
