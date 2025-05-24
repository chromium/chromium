// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/gaia_id.h"

#include <ostream>
#include <string>
#include <string_view>

GaiaId::GaiaId(std::string value) : id_(std::move(value)) {}

bool GaiaId::empty() const {
  return id_.empty();
}

const std::string& GaiaId::ToString() const {
  return id_;
}

std::ostream& operator<<(std::ostream& out, const GaiaId& id) {
  return out << id.ToString();
}
