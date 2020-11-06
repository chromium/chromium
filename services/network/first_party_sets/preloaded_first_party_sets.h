// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_FIRST_PARTY_SETS_PRELOADED_FIRST_PARTY_SETS_H_
#define SERVICES_NETWORK_FIRST_PARTY_SETS_PRELOADED_FIRST_PARTY_SETS_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "net/base/schemeful_site.h"
#include "services/network/first_party_sets/first_party_set_parser.h"

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

  void SetManuallySpecifiedSet(const std::string& flag_value);

  // Overwrites the current members-to-owners map with the values in |raw_sets|,
  // which should be the JSON-encoded string representation of a collection of
  // set declarations according to the format specified in this document:
  // https://github.com/privacycg/first-party-sets. Returns a pointer to the
  // mapping, for testing.
  //
  // In case of invalid input, clears the current members-to-owners map, but
  // keeps any manually-specified set (i.e. a set provided on the command line).
  base::flat_map<net::SchemefulSite, net::SchemefulSite>* ParseAndSet(
      base::StringPiece raw_sets);

  int64_t size() const { return sets_.size(); }

 private:
  // We must ensure there's no intersection between the manually-specified set
  // and the sets that came from Component Updater. (When reconciling the
  // manually-specified set and `sets_`, entries in the manually-specified set
  // always win.) We must also ensure that `sets_` includes the set described by
  // `manually_specified_set_`.
  void ApplyManuallySpecifiedSet();

  base::flat_map<net::SchemefulSite, net::SchemefulSite> sets_;
  base::Optional<
      std::pair<net::SchemefulSite, base::flat_set<net::SchemefulSite>>>
      manually_specified_set_;
};

}  // namespace network

#endif  // SERVICES_NETWORK_FIRST_PARTY_SETS_PRELOADED_FIRST_PARTY_SETS_H_
