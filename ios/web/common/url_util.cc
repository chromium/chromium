// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/common/url_util.h"

namespace web {

GURL GURLByRemovingRefFromGURL(const GURL& full_url) {
  if (!full_url.has_ref())
    return full_url;

  GURL::Replacements replacements;
  replacements.ClearRef();
  return full_url.ReplaceComponents(replacements);
}

}  // namespace web
