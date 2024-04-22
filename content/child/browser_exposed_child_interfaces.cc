// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/browser_exposed_child_interfaces.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "components/metrics/child_histogram_fetcher_impl.h"
#include "content/child/child_process_synthetic_trial_syncer.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/binder_map.h"
#include "services/tracing/public/cpp/traced_process.h"
#include "services/tracing/public/mojom/traced_process.mojom.h"

namespace content {

void ExposeChildInterfacesToBrowser(
    scoped_refptr<base::SequencedTaskRunner> io_task_runner,
    const bool in_browser_process,
    mojo::BinderMap* binders) {
  binders->Add<metrics::mojom::ChildHistogramFetcherFactory>(
      base::BindRepeating(&metrics::ChildHistogramFetcherFactoryImpl::Create),
      io_task_runner);
  binders->Add<tracing::mojom::TracedProcess>(
      base::BindRepeating(&tracing::TracedProcess::OnTracedProcessRequest),
      base::SequencedTaskRunner::GetCurrentDefault());

  // TODO(crbug.com/40946277): Investiagte the reason why the mojo connection
  // is often created and closed for the same render process on lacros-chrome.
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
  if (!in_browser_process) {
    binders->Add<mojom::SyntheticTrialConfiguration>(
        base::BindRepeating(&ChildProcessSyntheticTrialSyncer::Create),
        base::SequencedTaskRunner::GetCurrentDefault());
  }
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)

  GetContentClient()->ExposeInterfacesToBrowser(io_task_runner, binders);
}

}  // namespace content
