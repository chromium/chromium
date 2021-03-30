// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_IMPL_H_
#define CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_IMPL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/strings/string_piece_forward.h"
#include "content/common/content_export.h"
#include "content/public/browser/mojo_binder_policy_map.h"

namespace content {

// Implements MojoBinderPolicyMap and owns a policy map.
class CONTENT_EXPORT MojoBinderPolicyMapImpl : public MojoBinderPolicyMap {
 public:
  MojoBinderPolicyMapImpl();
  explicit MojoBinderPolicyMapImpl(
      const base::flat_map<std::string, MojoBinderPolicy>& init_map);
  ~MojoBinderPolicyMapImpl() override;

  // Disallows copy and move operations.
  MojoBinderPolicyMapImpl(const MojoBinderPolicyMapImpl& other) = delete;
  MojoBinderPolicyMapImpl& operator=(const MojoBinderPolicyMapImpl& other) =
      delete;
  MojoBinderPolicyMapImpl(MojoBinderPolicyMapImpl&&) = delete;
  MojoBinderPolicyMapImpl& operator=(MojoBinderPolicyMapImpl&&) = delete;

  // Returns the instance used by MojoBinderPolicyApplier for prerendering
  // pages.
  // This is used when the prerendered page and the page that triggered the
  // prerendering are same origin. Currently this is the only use of this class.
  static const MojoBinderPolicyMapImpl* GetInstanceForSameOriginPrerendering();

  // Gets the corresponding policy of a given Mojo interface name. If the
  // interface name is not in `policy_map_`, the given `default_policy` will be
  // returned.
  MojoBinderPolicy GetMojoBinderPolicy(
      const std::string& interface_name,
      const MojoBinderPolicy default_policy) const;
  // Fails with DCHECK if the interface is not in the map.
  MojoBinderPolicy GetMojoBinderPolicyOrDieForTesting(
      const std::string& interface_name) const;

 private:
  // MojoBinderPolicyMap implementation:
  void SetPolicyByName(const base::StringPiece& name,
                       MojoBinderPolicy policy) override;

  base::flat_map<std::string, MojoBinderPolicy> policy_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_IMPL_H_
