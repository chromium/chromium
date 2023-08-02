// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/named_guide_util.h"

#import "ios/chrome/browser/shared/ui/util/named_guide.h"

void AddNamedGuidesToView(NSArray<GuideName*>* names, UIView* view) {
  for (GuideName* name in names) {
    [view addLayoutGuide:[[NamedGuide alloc] initWithName:name]];
  }
}
