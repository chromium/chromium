// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_IMPL_H_
#define CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_IMPL_H_

#include <string>
#include <string_view>

#include "base/containers/flat_map.h"
#include "content/common/content_export.h"
#include "content/public/browser/mojo_binder_policy_map.h"

namespace content {

// Implements MojoBinderPolicyMap and owns a policy map.
class CONTENT_EXPORT MojoBinderPolicyMapImpl : public MojoBinderPolicyMap {
 public:
  MojoBinderPolicyMapImpl();

  // This constructor is for testing.
  explicit MojoBinderPolicyMapImpl(
      const base::flat_map<std::string, MojoBinderNonAssociatedPolicy>&
          init_map);
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

  // Returns the instance used by MojoBinderPolicyApplier for preview mode. This
  // is used when a page is shown in preview mode.
  static const MojoBinderPolicyMapImpl* GetInstanceForPreview();

  // Gets the corresponding policy of a given Mojo interface name.
  // If the interface name is not in `non_associated_policy_map_`, the given
  // `default_policy` will be returned.
  // Callers should ensure that the corresponding interface is used as a
  // non-associated interface in the context. If the interface is used as a
  // channel-associated interface, they should call
  // `GetAssociatedMojoBinderPolicy`.
  MojoBinderNonAssociatedPolicy GetNonAssociatedMojoBinderPolicy(
      const std::string& interface_name,
      const MojoBinderNonAssociatedPolicy default_policy) const;

  // Gets the corresponding policy of a given Mojo interface name.
  // If the interface name is not in `associated_policy_map_`, the given
  // `default_policy` will be returned.
  // Callers should ensure that the corresponding interface is used as a
  // channel-associated interface in the context. If the interface is used as a
  // non-associated interface, they should call
  // `GetNonAssociatedMojoBinderPolicy`.
  MojoBinderAssociatedPolicy GetAssociatedMojoBinderPolicy(
      const std::string& interface_name,
      const MojoBinderAssociatedPolicy default_policy) const;

  // Fails with DCHECK if the interface is not in `non_associated_policy_map_`.
  MojoBinderNonAssociatedPolicy GetNonAssociatedMojoBinderPolicyOrDieForTesting(
      const std::string& interface_name) const;

  // Fails with DCHECK if the interface is not in `associated_policy_map_`.
  MojoBinderAssociatedPolicy GetAssociatedMojoBinderPolicyOrDieForTesting(
      const std::string& interface_name) const;

 private:
  // MojoBinderPolicyMap implementation:
  void SetPolicyByName(const std::string_view& name,
                       MojoBinderAssociatedPolicy policy) override;

  void SetPolicyByName(const std::string_view& name,
                       MojoBinderNonAssociatedPolicy policy) override;

  base::flat_map<std::string, MojoBinderNonAssociatedPolicy>
      non_associated_policy_map_;
  base::flat_map<std::string, MojoBinderAssociatedPolicy>
      associated_policy_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_MOJO_BINDER_POLICY_MAP_IMPL_H_
