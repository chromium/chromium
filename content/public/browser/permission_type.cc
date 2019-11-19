// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/permission_type.h"

#include "base/no_destructor.h"
#include "base/stl_util.h"

namespace content {

const std::vector<PermissionType>& GetAllPermissionTypes() {
  static const base::NoDestructor<std::vector<PermissionType>>
      kAllPermissionTypes([] {
        const int NUM_TYPES = static_cast<int>(PermissionType::NUM);
        std::vector<PermissionType> all_types;
        all_types.reserve(NUM_TYPES - 2);
        for (int i = 1; i < NUM_TYPES; ++i) {
          if (i == 2)  // Skip PUSH_MESSAGING.
            continue;
          all_types.push_back(static_cast<PermissionType>(i));
        }
        return all_types;
      }());
  return *kAllPermissionTypes;
}

}  // namespace content
