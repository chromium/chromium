// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_PRELOADED_FIRST_PARTY_SETS_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_PRELOADED_FIRST_PARTY_SETS_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"

namespace network {

// Class PreloadedFirstPartySets is a pseudo-singleton owned by NetworkService;
// it stores all known information about preloaded First-Party Sets state. This
// information is updated by the component updater via |ParseAndSet|.
class PreloadedFirstPartySets {
 public:
  PreloadedFirstPartySets();
  ~PreloadedFirstPartySets();

  PreloadedFirstPartySets(const PreloadedFirstPartySets&) = delete;
  PreloadedFirstPartySets& operator=(const PreloadedFirstPartySets&) = delete;

  // Overwrites the current owners-to-sets map with the values in |raw_sets|,
  // which should be the JSON-encoded string representation of a collection of
  // set declarations according to the format specified in this document:
  // https://github.com/privacycg/first-party-sets
  void ParseAndSet(base::StringPiece raw_sets);

 private:
  base::flat_map<std::string, std::string> sets_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_PRELOADED_FIRST_PARTY_SETS_H_
