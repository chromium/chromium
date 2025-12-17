// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/data_import/public/utils.h"

#import "base/strings/sys_string_conversions.h"
#import "components/url_formatter/elide_url.h"
#import "components/url_formatter/url_fixer.h"
#import "ios/chrome/browser/shared/ui/util/url_with_title.h"

URLWithTitle* GetURLWithTitleForURLString(const std::string& url_string) {
  GURL url = url_formatter::FixupURL(url_string, std::string());
  if (url.is_empty()) {
    return nil;
  }
  NSString* title = base::SysUTF16ToNSString(
      url_formatter::
          FormatUrlForDisplayOmitSchemePathTrivialSubdomainsAndMobilePrefix(
              url));
  return [[URLWithTitle alloc] initWithURL:url title:title];
}
