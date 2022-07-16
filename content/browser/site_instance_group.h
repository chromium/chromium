// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SITE_INSTANCE_GROUP_H_
#define CONTENT_BROWSER_SITE_INSTANCE_GROUP_H_

#include "base/memory/ref_counted.h"
#include "base/types/id_type.h"

namespace content {

using SiteInstanceGroupId = base::IdType32<class SiteInstanceGroupIdTag>;

// A SiteInstanceGroup represents one view of a browsing context group's frame
// trees within a renderer process. It provides a tuning knob, allowing the
// number of groups to vary (for process allocation and
// painting/input/scheduling decisions) without affecting the number of security
// principals that are tracked with SiteInstances.
//
// Similar to layers composing an image from many colors, a set of
// SiteInstanceGroups compose a web page from many renderer processes. Each
// group represents one renderer process' view of a browsing context group,
// containing both local frames (organized into widgets of contiguous frames)
// and proxies for frames in other groups or processes.
//
// The documents in the local frames of a group are organized into
// SiteInstances, representing an atomic group of similar origin documents that
// can access each other directly. A group contains all the documents of one or
// more SiteInstances, all belonging to the same browsing context group (aka
// BrowsingInstance). Each browsing context group has its own set of
// SiteInstanceGroups.
//
// A SiteInstanceGroup is used for generating painted surfaces, directing input
// events, and facilitating communication between frames in different groups.
// The browser process coordinates activities across groups to produce a full
// web page.
//
// SiteInstanceGroups are refcounted by the SiteInstances using them, allowing
// for flexible policies.  Currently, each SiteInstanceGroup has exactly one
// SiteInstance.  See crbug.com/1195535.
class SiteInstanceGroup : public base::RefCounted<SiteInstanceGroup> {
 public:
  SiteInstanceGroup();

  SiteInstanceGroup(const SiteInstanceGroup&) = delete;
  SiteInstanceGroup& operator=(const SiteInstanceGroup&) = delete;

  SiteInstanceGroupId GetId();

 private:
  friend class RefCounted<SiteInstanceGroup>;
  ~SiteInstanceGroup();

  // A unique ID for this SiteInstanceGroup.
  SiteInstanceGroupId id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SITE_INSTANCE_GROUP_H_
