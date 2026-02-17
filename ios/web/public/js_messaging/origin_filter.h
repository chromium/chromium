// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_JS_MESSAGING_ORIGIN_FILTER_H_
#define IOS_WEB_PUBLIC_JS_MESSAGING_ORIGIN_FILTER_H_

#import <Foundation/Foundation.h>

namespace web {

// The origin filter to restrict the script injection.
enum class OriginFilter {
  kPublic = 0,
  // A temporary origin filter to use for test. Once a real filter has been
  // added, remove the temporary origin and use the new origins in tests.
  // TODO(crbug.com/481255908): Remove the placeholder filter.
  kValidTestOriginForTesting,
};

// Converts `filter` in the actual list of allowed origins.
// Returns `nil` for `kPublic`.
NSArray<NSString*>* GetOriginList(web::OriginFilter filter);

}  // namespace web

#endif  // IOS_WEB_PUBLIC_JS_MESSAGING_ORIGIN_FILTER_H_
