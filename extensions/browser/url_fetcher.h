// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_FETCHER_H_
#define EXTENSIONS_BROWSER_URL_FETCHER_H_

namespace extensions {

class URLFetcher {
 public:
  virtual ~URLFetcher() = default;

  virtual void Start() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_FETCHER_H_
