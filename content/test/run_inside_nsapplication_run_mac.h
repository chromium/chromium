// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_RUN_INSIDE_NSAPPLICATION_RUN_MAC_H_
#define CONTENT_TEST_RUN_INSIDE_NSAPPLICATION_RUN_MAC_H_

#include "base/functional/callback_forward.h"

namespace content {

// Runs `closure` synchronously from inside -[NSApplication run], so that
// NSApp.running is YES while `closure` runs, as it is for all UI code in
// production. Returns after `closure` has run and -[NSApplication run] has
// returned.
//
// `closure` does not run inside a base::RunLoop: a base::RunLoop run by
// `closure` is not nested, so non-nestable tasks still run in it. Chromium
// tasks only run while `closure` spins a base::RunLoop, as before.
//
// NSApp must not be running yet.
//
// See https://crbug.com/570104905.
void RunInsideNSApplicationRun(base::OnceClosure closure);

}  // namespace content

#endif  // CONTENT_TEST_RUN_INSIDE_NSAPPLICATION_RUN_MAC_H_
