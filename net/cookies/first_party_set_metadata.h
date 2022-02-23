// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_FIRST_PARTY_SET_METADATA_H_
#define NET_COOKIES_FIRST_PARTY_SET_METADATA_H_

#include "base/stl_util.h"
#include "net/base/net_export.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/same_party_context.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace net {

// This class bundles together metadata about the First-Party Set associated
// with a given context.
class NET_EXPORT FirstPartySetMetadata {
 public:
  FirstPartySetMetadata();

  // `frame_owner` and `top_frame_owner` must live for the duration of the ctor;
  // nullptr indicates that there's no First-Party Set that's associated with
  // the current frame or the top frame, respectively, in the given context.
  FirstPartySetMetadata(
      const SamePartyContext& context,
      const SchemefulSite* frame_owner,
      const SchemefulSite* top_frame_owner,
      FirstPartySetsContextType first_party_sets_context_type);

  FirstPartySetMetadata(FirstPartySetMetadata&&);
  FirstPartySetMetadata& operator=(FirstPartySetMetadata&&);

  ~FirstPartySetMetadata();

  bool operator==(const FirstPartySetMetadata& other) const;

  const SamePartyContext& context() const { return context_; }

  // Returns a optional<T>& instead of a T* so that operator== can be defined
  // more easily.
  const absl::optional<SchemefulSite>& frame_owner() const {
    return frame_owner_;
  }
  const absl::optional<SchemefulSite>& top_frame_owner() const {
    return top_frame_owner_;
  }

  FirstPartySetsContextType first_party_sets_context_type() const {
    return first_party_sets_context_type_;
  }

 private:
  SamePartyContext context_ = SamePartyContext();
  absl::optional<SchemefulSite> frame_owner_ = absl::nullopt;
  absl::optional<SchemefulSite> top_frame_owner_ = absl::nullopt;
  FirstPartySetsContextType first_party_sets_context_type_ =
      FirstPartySetsContextType::kUnknown;
};

NET_EXPORT std::ostream& operator<<(std::ostream& os,
                                    const FirstPartySetMetadata& fpsm);

}  // namespace net

#endif  // NET_COOKIES_FIRST_PARTY_SET_METADATA_H_
