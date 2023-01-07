// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_BROWSER_EXPOSED_UTILITY_INTERFACES_H_
#define CONTENT_UTILITY_BROWSER_EXPOSED_UTILITY_INTERFACES_H_

namespace mojo {
class BinderMap;
}

namespace content {

// Registers with |binders| any interfaces exposed directly to the browser
// process by utility (i.e. service) processes. Interfaces exposed here are
// accessible to the browser via |BindReceiver()| on a UtilityProcessHost's
// underlying ChildProcessHost object.
void ExposeUtilityInterfacesToBrowser(mojo::BinderMap* binders);

}  // namespace content

#endif  // CONTENT_UTILITY_BROWSER_EXPOSED_UTILITY_INTERFACES_H_
