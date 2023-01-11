// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_HISTOGRAM_FETCHER_H_
#define CONTENT_PUBLIC_BROWSER_HISTOGRAM_FETCHER_H_

#include "base/functional/callback.h"
#include "base/task/task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"

namespace content {

// Fetch histogram data asynchronously from the various child processes, into
// the browser process. This method is used by the metrics services in
// preparation for a log upload. It contacts all processes, and get them to
// upload to the browser any/all changes to histograms.  When all changes have
// been acquired, or when the wait time expires (whichever is sooner), post the
// callback to the specified TaskRunner. Note the callback is posted exactly
// once.
CONTENT_EXPORT void FetchHistogramsAsynchronously(
    scoped_refptr<base::TaskRunner> task_runner,
    base::OnceClosure callback,
    base::TimeDelta wait_time);

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_HISTOGRAM_FETCHER_H_
