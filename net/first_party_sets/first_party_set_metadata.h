// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_METADATA_H_
#define NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_METADATA_H_

#include "net/base/net_export.h"
#include "net/first_party_sets/first_party_set_entry.h"
#include "net/first_party_sets/same_party_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// This class bundles together metadata about the First-Party Set associated
// with a given context.
class NET_EXPORT FirstPartySetMetadata {
 public:
  FirstPartySetMetadata();

  // `frame_entry` and `top_frame_entry` must live for the duration of the ctor;
  // nullptr indicates that there's no First-Party Set that's associated with
  // the current frame or the top frame, respectively, in the given context.
  FirstPartySetMetadata(const SamePartyContext& context,
                        const FirstPartySetEntry* frame_entry,
                        const FirstPartySetEntry* top_frame_entry);

  FirstPartySetMetadata(FirstPartySetMetadata&&);
  FirstPartySetMetadata& operator=(FirstPartySetMetadata&&);

  ~FirstPartySetMetadata();

  bool operator==(const FirstPartySetMetadata& other) const;
  bool operator!=(const FirstPartySetMetadata& other) const;

  const SamePartyContext& context() const { return context_; }

  // Returns a optional<T>& instead of a T* so that operator== can be defined
  // more easily.
  const absl::optional<FirstPartySetEntry>& frame_entry() const {
    return frame_entry_;
  }
  const absl::optional<FirstPartySetEntry>& top_frame_entry() const {
    return top_frame_entry_;
  }

  // Returns true if `frame_entry` and `top_frame_entry` are both non-null and
  // have the same primary. This is different from `context_.context_type()`
  // because it only checks if the the frames' sites are in the same set
  // regardless of their ancestor chain.
  bool AreSitesInSameFirstPartySet() const;

 private:
  SamePartyContext context_ = SamePartyContext();
  absl::optional<FirstPartySetEntry> frame_entry_ = absl::nullopt;
  absl::optional<FirstPartySetEntry> top_frame_entry_ = absl::nullopt;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const FirstPartySetMetadata& fpsm);

}  // namespace net

#endif  // NET_FIRST_PARTY_SETS_FIRST_PARTY_SET_METADATA_H_
