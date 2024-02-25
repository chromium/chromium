// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_ENTRY_OVERRIDE_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_ENTRY_OVERRIDE_H_

#include <optional>

#include "net/base/net_export.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace mojo {
template <typename DataViewType, typename T>
struct StructTraits;
}  // namespace mojo
namespace network::mojom {
class FirstPartySetEntryOverrideDataView;
}  // namespace network::mojom

namespace net {

// This class represents a single modification to be applied on top of the
// global First-Party Sets list. A modifications may be a deletion, remapping,
// or new mapping.
class NET_EXPORT FirstPartySetEntryOverride {
 public:
  // Creates a new modification representing a deletion.
  FirstPartySetEntryOverride();
  // Creates a new modification representing a remapping/additional mapping.
  explicit FirstPartySetEntryOverride(FirstPartySetEntry entry);

  FirstPartySetEntryOverride(FirstPartySetEntryOverride&& other);
  FirstPartySetEntryOverride& operator=(FirstPartySetEntryOverride&& other);
  FirstPartySetEntryOverride(const FirstPartySetEntryOverride& other);
  FirstPartySetEntryOverride& operator=(
      const FirstPartySetEntryOverride& other);

  ~FirstPartySetEntryOverride();

  bool operator==(const FirstPartySetEntryOverride& other) const;

  // Returns true iff this override is a deletion.
  bool IsDeletion() const { return !entry_.has_value(); }

  // Returns the new target entry, if this override is not a deletion. Must not
  // be called if `IsDeletion()` is true.
  const FirstPartySetEntry& GetEntry() const {
    CHECK(!IsDeletion());
    return entry_.value();
  }

 private:
  // mojo (de)serialization needs access to private details.
  friend struct mojo::StructTraits<
      network::mojom::FirstPartySetEntryOverrideDataView,
      FirstPartySetEntryOverride>;

  std::optional<FirstPartySetEntry> entry_;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const FirstPartySetEntryOverride& override);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_ENTRY_OVERRIDE_H_