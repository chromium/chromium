// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_TESTING_NSERROR_UTIL_H_
#define IOS_TESTING_NSERROR_UTIL_H_

@class NSError;
@class NSString;

namespace testing {

// Returns a NSError with generic domain and error code, and the provided string
// as localizedDescription.
NSError* NSErrorWithLocalizedDescription(NSString* error_description);

}  // namespace testing

#endif  // IOS_TESTING_NSERROR_UTIL_H_
