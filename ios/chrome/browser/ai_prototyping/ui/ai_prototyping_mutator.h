// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_MUTATOR_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_MUTATOR_H_

#import "components/optimization_guide/optimization_guide_buildflags.h"

namespace optimization_guide {
namespace proto {
class BlingPrototypingRequest;
class StringValue;
enum TabOrganizationRequest_TabOrganizationModelStrategy : int;
}  // namespace proto
}  // namespace optimization_guide

// Mutator protocol for the UI layer to communicate to the
// AIPrototypingMediator.
@protocol AIPrototypingMutator

#if BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)
// Executes a prototyping request to a server-hosted model.
- (void)executeServerQuery:
    (optimization_guide::proto::BlingPrototypingRequest)request;

// Executes a prototyping request to an on-device model.
- (void)executeOnDeviceQuery:(optimization_guide::proto::StringValue)request;

// Executes a tab organization request with a given organization `strategy`.
- (void)executeGroupTabsWithStrategy:
    (optimization_guide::proto::
         TabOrganizationRequest_TabOrganizationModelStrategy)strategy;
#endif  // BUILDFLAG(BUILD_WITH_INTERNAL_OPTIMIZATION_GUIDE)

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_MUTATOR_H_
