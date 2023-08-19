// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"

@implementation OpenLensInputSelectionCommand

- (instancetype)initWithEntryPoint:(LensEntrypoint)entryPoint
                 presentationStyle:
                     (LensInputSelectionPresentationStyle)presentationStyle
            presentationCompletion:(ProceduralBlock)presentationCompletion {
  self = [super init];
  if (self) {
    _entryPoint = entryPoint;
    _presentationStyle = presentationStyle;
    _presentationCompletion = presentationCompletion;
  }
  return self;
}

@end
