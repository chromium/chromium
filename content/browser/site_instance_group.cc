// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/site_instance_group.h"

namespace content {

namespace {
SiteInstanceGroupId::Generator site_instance_group_id_generator;
}  // namespace

SiteInstanceGroup::SiteInstanceGroup()
    : id_(site_instance_group_id_generator.GenerateNextId()) {}

SiteInstanceGroup::~SiteInstanceGroup() = default;

SiteInstanceGroupId SiteInstanceGroup::GetId() {
  return id_;
}

}  // namespace content
