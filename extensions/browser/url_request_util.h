// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
#define EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_

#include <string>

#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "content/public/common/resource_type.h"
#include "ui/base/page_transition_types.h"

class GURL;

namespace extensions {
class Extension;
class ExtensionSet;
class ProcessMap;

// Utilities related to URLRequest jobs for extension resources. See
// chrome/browser/extensions/extension_protocols_unittest.cc for related tests.
namespace url_request_util {

// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoad(const GURL& url,
                                    content::ResourceType resource_type,
                                    ui::PageTransition page_transition,
                                    int child_id,
                                    bool is_incognito,
                                    const Extension* extension,
                                    const ExtensionSet& extensions,
                                    const ProcessMap& process_map,
                                    bool* allowed);

// Helper method that is called by both AllowCrossRendererResourceLoad and
// ExtensionNavigationThrottle to share logic.
// Sets allowed=true to allow a chrome-extension:// resource request coming from
// renderer A to access a resource in an extension running in renderer B.
// Returns false when it couldn't determine if the resource is allowed or not
bool AllowCrossRendererResourceLoadHelper(bool is_guest,
                                          const Extension* extension,
                                          const Extension* owner_extension,
                                          const std::string& partition_id,
                                          base::StringPiece resource_path,
                                          ui::PageTransition page_transition,
                                          bool* allowed);

// Checks whether the given |extension| and |resource_path| are part of a
// special case where an extension URL is permitted to load in any guest
// process, rather than only in guests of a given platform app. If
// |resource_path| is base::nullopt, then the check is based solely on which
// extension is passed in, allowing this to be used for origin checks as well as
// URL checks.
// TODO(creis): Remove this method when the special cases (listed by bug number
// in the definition of this method) are gone.
bool AllowSpecialCaseExtensionURLInGuest(
    const Extension* extension,
    base::Optional<base::StringPiece> resource_path);

}  // namespace url_request_util
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_REQUEST_UTIL_H_
