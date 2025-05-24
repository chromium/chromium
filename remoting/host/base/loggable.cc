// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/base/loggable.h"

#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/location.h"
#include "base/types/expected.h"

namespace remoting {

class Loggable::Inner {
 public:
  Inner(base::Location from_here, std::string message)
      : from_here(std::move(from_here)), message(std::move(message)) {}

  Inner(const Inner&) = default;
  Inner(Inner&&) = default;
  Inner& operator=(const Inner&) = default;
  Inner& operator=(Inner&&) = default;

  base::Location from_here;
  std::string message;
  std::vector<std::pair<base::Location, std::string>> context;
};

Loggable::Loggable() = default;

Loggable::Loggable(base::Location from_here, std::string message)
    : inner_(
          std::make_unique<Inner>(std::move(from_here), std::move(message))) {}

Loggable::Loggable(const Loggable& other)
    : inner_(std::make_unique<Inner>(*other.inner_)) {}

Loggable::Loggable(Loggable&&) = default;

Loggable& Loggable::operator=(const Loggable& other) {
  if (this != &other) {
    inner_ = std::make_unique<Inner>(*other.inner_);
  }
  return *this;
}

Loggable& Loggable::operator=(Loggable&&) = default;

Loggable::~Loggable() = default;

void Loggable::AddContext(base::Location from_here, std::string context) {
  CHECK(inner_);
  inner_->context.emplace_back(std::move(from_here), std::move(context));
}

base::unexpected<Loggable> Loggable::UnexpectedWithContext(
    base::Location from_here,
    std::string context) && {
  AddContext(std::move(from_here), std::move(context));
  return base::unexpected(std::move(*this));
}

std::string Loggable::ToString() const {
  std::ostringstream str;
  str << *this;
  return str.str();
}

std::ostream& operator<<(std::ostream& ostream, const Loggable& loggable) {
  CHECK(loggable.inner_);
  ostream << "[" << loggable.inner_->from_here.file_name() << ":"
          << loggable.inner_->from_here.line_number() << "] "
          << loggable.inner_->message;
  for (auto& [location, context] : loggable.inner_->context) {
    ostream << std::endl
            << "... [" << location.file_name() << ":" << location.line_number()
            << "] " << context;
  }
  return ostream;
}

}  // namespace remoting
