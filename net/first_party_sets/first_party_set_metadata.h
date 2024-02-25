// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_METADATA_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_METADATA_H_

#include <optional>

#include "net/base/net_export.h"
#include "net/first_party_sets/first_party_set_entry.h"

namespace net {

// This class bundles together metadata about the First-Party Set associated
// with a given context.
class NET_EXPORT FirstPartySetMetadata {
 public:
  FirstPartySetMetadata();

  // `frame_entry` and `top_frame_entry` must live for the duration of the ctor;
  // nullptr indicates that there's no First-Party Set that's associated with
  // the current frame or the top frame, respectively, in the given context.
  FirstPartySetMetadata(const FirstPartySetEntry* frame_entry,
                        const FirstPartySetEntry* top_frame_entry);

  FirstPartySetMetadata(FirstPartySetMetadata&&);
  FirstPartySetMetadata& operator=(FirstPartySetMetadata&&);

  ~FirstPartySetMetadata();

  bool operator==(const FirstPartySetMetadata& other) const;
  bool operator!=(const FirstPartySetMetadata& other) const;

  const std::optional<FirstPartySetEntry>& frame_entry() const {
    return frame_entry_;
  }
  const std::optional<FirstPartySetEntry>& top_frame_entry() const {
    return top_frame_entry_;
  }

  // Returns true if `frame_entry` and `top_frame_entry` are both non-null and
  // have the same primary.
  bool AreSitesInSameFirstPartySet() const;

 private:
  std::optional<FirstPartySetEntry> frame_entry_ = std::nullopt;
  std::optional<FirstPartySetEntry> top_frame_entry_ = std::nullopt;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const FirstPartySetMetadata& fpsm);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_METADATA_H_
