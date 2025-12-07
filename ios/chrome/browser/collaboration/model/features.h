// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COLLABORATION_MODEL_FEATURES_H_
#define IOS_CHROME_BROWSER_COLLABORATION_MODEL_FEATURES_H_

namespace collaboration {
class CollaborationService;
}  // namespace collaboration

// Whether the Shared Tab Groups feature is enabled and a user can join to an
// existing shared group.
bool IsSharedTabGroupsJoinEnabled(
    collaboration::CollaborationService* collaboration_service);

// Whether the Shared Tab Groups feature is enabled and a user can create a new
// shared group.
bool IsSharedTabGroupsCreateEnabled(
    collaboration::CollaborationService* collaboration_service);

#endif  // IOS_CHROME_BROWSER_COLLABORATION_MODEL_FEATURES_H_
