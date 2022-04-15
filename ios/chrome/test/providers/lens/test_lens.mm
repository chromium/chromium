// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/lens/lens_api.h"
#import "ios/public/provider/chrome/browser/lens/lens_configuration.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios {
namespace provider {
namespace {

// The domain for NSErrors.
NSErrorDomain const kTestLensProviderErrorDomain =
    @"kTestLensProviderErrorDomain";

// The error codes for kTestLensProviderErrorDomain.
enum TestLensProviderErrors : NSInteger {
  kTestLensProviderErrorNotImplemented,
};

}

id<ChromeLensController> NewChromeLensController(LensConfiguration* config) {
  // Lens is not supported for tests.
  return nil;
}

bool IsLensSupported() {
  // Lens is not supported for tests.
  return false;
}

void GenerateLensWebURLForImage(
    UIImage* image,
    ios::provider::LensWebURLCompletion completion) {
  NSError* error = [NSError errorWithDomain:kTestLensProviderErrorDomain
                                       code:kTestLensProviderErrorNotImplemented
                                   userInfo:nil];
  base::SequencedTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                   base::BindOnce(^() {
                                                     completion(nil, error);
                                                   }));
}

}  // namespace provider
}  // namespace ios
