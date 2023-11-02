// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/installability_error.h"

#include <utility>

namespace content {

InstallabilityErrorArgument::InstallabilityErrorArgument(std::string name,
                                                         std::string value)
    : name(std::move(name)), value(std::move(value)) {}

bool InstallabilityErrorArgument::operator==(
    const InstallabilityErrorArgument& other) const {
  return name == other.name && value == other.value;
}

InstallabilityErrorArgument::~InstallabilityErrorArgument() = default;

InstallabilityError::InstallabilityError() = default;

InstallabilityError::InstallabilityError(std::string error_id)
    : error_id(std::move(error_id)) {}

InstallabilityError::InstallabilityError(const InstallabilityError& other) =
    default;

InstallabilityError::InstallabilityError(InstallabilityError&& other) = default;

bool InstallabilityError::operator==(const InstallabilityError& other) const {
  return error_id == other.error_id &&
         installability_error_arguments == other.installability_error_arguments;
}

InstallabilityError::~InstallabilityError() = default;

std::ostream& operator<<(std::ostream& os, const InstallabilityError& error) {
  os << "[" << error.error_id;
  for (const auto& arg : error.installability_error_arguments)
    os << ", " << arg.name << "=" << arg.value;
  os << "]";

  return os;
}

}  // namespace content
