// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAM_CHILD_PROCESS_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAM_CHILD_PROCESS_H_

#include "content/common/histogram_fetcher.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace content {

// Interface that is implemented by the various child process types that can
// be added to HistogramController for collecting histogram data.
class HistogramChildProcess {
 public:
  // Called to connect to a ChildHistogramFetcherFactory implementation in the
  // child process.
  virtual void BindChildHistogramFetcherFactory(
      mojo::PendingReceiver<content::mojom::ChildHistogramFetcherFactory>
          factory) = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAM_CHILD_PROCESS_H_
