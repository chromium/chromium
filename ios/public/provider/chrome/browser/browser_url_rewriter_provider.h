// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_URL_REWRITER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_URL_REWRITER_PROVIDER_H_

#include "ios/web/public/navigation/browser_url_rewriter.h"

#include "base/macros.h"

// Provider class for custom BrowserURLRewriter.
class BrowserURLRewriterProvider {
 public:
  BrowserURLRewriterProvider();
  virtual ~BrowserURLRewriterProvider();
  // Adds the provider rewriters into |rewriter|.
  virtual void AddProviderRewriters(web::BrowserURLRewriter* rewriter);

 private:
  DISALLOW_COPY_AND_ASSIGN(BrowserURLRewriterProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_BROWSER_URL_REWRITER_PROVIDER_H_
