// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/preloaded_first_party_sets.h"
#include <memory>

#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/values.h"
#include "services/network/first_party_sets/first_party_set_parser.h"
#include "services/network/public/cpp/network_switches.h"

namespace network {

PreloadedFirstPartySets::PreloadedFirstPartySets() = default;

PreloadedFirstPartySets::~PreloadedFirstPartySets() = default;

void PreloadedFirstPartySets::ParseAndSet(base::StringPiece raw_sets) {
  std::unique_ptr<base::flat_map<std::string, std::string>> parsed =
      FirstPartySetParser::ParsePreloadedSets(raw_sets);
  if (parsed)
    sets_.swap(*parsed);
}

}  // namespace network
