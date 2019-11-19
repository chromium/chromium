// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/history/history_utils.h"

#include "components/dom_distiller/core/url_constants.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace ios {

// Returns true if this looks like the type of URL that should be added to the
// history. This filters out URLs such a JavaScript.
bool CanAddURLToHistory(const GURL& url) {
  if (!url.is_valid())
    return false;

  // TODO(crbug.com/1007192): Don't store the URL as we aren't persiting the
  // files. Maybe we should start persisting the files and store the URL.
  // TODO: We should allow ChromeUIScheme URLs if they have been explicitly
  // typed.  Right now, however, these are marked as typed even when triggered
  // by a shortcut or menu action.
  if (url.SchemeIs(url::kJavaScriptScheme) ||
      url.SchemeIs(dom_distiller::kDomDistillerScheme) ||
      url.SchemeIs(kChromeUIScheme) || url.SchemeIs(url::kFileScheme))
    return false;

  // Allow all about: URLs except about:blank|newtab.
  if (url == url::kAboutBlankURL || url == kChromeUIAboutNewTabURL)
    return false;

  return true;
}

}  // namespace ios
