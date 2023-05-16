// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/metrics/histogram_controller.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/process/process_handle.h"
#include "base/rand_util.h"
#include "content/browser/metrics/histogram_subscriber.h"
#include "content/common/histogram_fetcher.mojom-shared.h"
#include "content/common/histogram_fetcher.mojom.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/process_type.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"

namespace content {

namespace {
const char* GetPingHistogramName(mojom::UmaPingCallSource call_source) {
  switch (call_source) {
    case mojom::UmaPingCallSource::PERIODIC:
      return "UMA.ChildProcess.Ping.Periodic";
    case mojom::UmaPingCallSource::SHARED_MEMORY_SET_UP:
      return "UMA.ChildProcess.Ping.SharedMemorySetUp";
  }
}
}  // namespace

HistogramController* HistogramController::GetInstance() {
  return base::Singleton<HistogramController, base::LeakySingletonTraits<
                                                  HistogramController>>::get();
}

HistogramController::HistogramController() : subscriber_(nullptr) {
  // Unretained is safe because |this| is leaky.
  timer_.Start(FROM_HERE, base::Minutes(5),
               base::BindRepeating(&HistogramController::PingChildProcesses,
                                   base::Unretained(this)));
}

HistogramController::~HistogramController() = default;

void HistogramController::OnHistogramDataCollected(
    int sequence_number,
    const std::vector<std::string>& pickled_histograms) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&HistogramController::OnHistogramDataCollected,
                       base::Unretained(this), sequence_number,
                       pickled_histograms));
    return;
  }
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (subscriber_) {
    subscriber_->OnHistogramDataCollected(sequence_number, pickled_histograms);
  }
}

void HistogramController::Register(HistogramSubscriber* subscriber) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!subscriber_);
  subscriber_ = subscriber;
}

void HistogramController::Unregister(const HistogramSubscriber* subscriber) {
  DCHECK_EQ(subscriber_, subscriber);
  subscriber_ = nullptr;
}

template <class T>
void HistogramController::NotifyChildDied(T* host) {
  RemoveChildHistogramFetcherInterface(MayBeDangling<T>(host));
}

template void HistogramController::NotifyChildDied(RenderProcessHost* host);

template <>
HistogramController::ChildHistogramFetcherMap<ChildProcessHost>&
HistogramController::GetChildHistogramFetcherMap() {
  return child_histogram_fetchers_;
}

template <>
HistogramController::ChildHistogramFetcherMap<RenderProcessHost>&
HistogramController::GetChildHistogramFetcherMap() {
  return renderer_histogram_fetchers_;
}

template void HistogramController::SetHistogramMemory(
    ChildProcessHost* host,
    base::WritableSharedMemoryRegion shared_region);

template void HistogramController::SetHistogramMemory(
    RenderProcessHost* host,
    base::WritableSharedMemoryRegion shared_region);

template <class T>
void HistogramController::SetHistogramMemory(
    T* host,
    base::WritableSharedMemoryRegion shared_region) {
  mojo::Remote<content::mojom::ChildHistogramFetcherFactory> factory;
  host->BindReceiver(factory.BindNewPipeAndPassReceiver());

  mojo::Remote<content::mojom::ChildHistogramFetcher> fetcher;
  factory->CreateFetcher(std::move(shared_region),
                         fetcher.BindNewPipeAndPassReceiver());
  PingChildProcess(fetcher.get(),
                   mojom::UmaPingCallSource::SHARED_MEMORY_SET_UP);
  InsertChildHistogramFetcherInterface(host, std::move(fetcher));
}

template <class T>
void HistogramController::InsertChildHistogramFetcherInterface(
    T* host,
    mojo::Remote<content::mojom::ChildHistogramFetcher>
        child_histogram_fetcher) {
  // Broken pipe means remove this from the map. The map size is a proxy for
  // the number of known processes
  //
  // `RemoveChildHistogramFetcherInterface` will only use `host` for address
  // comparison without being dereferenced , therefore it's not going to create
  // a UAF.
  child_histogram_fetcher.set_disconnect_handler(base::BindOnce(
      &HistogramController::RemoveChildHistogramFetcherInterface<T>,
      base::Unretained(this), base::UnsafeDangling(host)));
  GetChildHistogramFetcherMap<T>()[host] = std::move(child_histogram_fetcher);
}

template <class T>
content::mojom::ChildHistogramFetcher*
HistogramController::GetChildHistogramFetcherInterface(T* host) {
  auto it = GetChildHistogramFetcherMap<T>().find(host);
  if (it != GetChildHistogramFetcherMap<T>().end()) {
    return (it->second).get();
  }
  return nullptr;
}

void HistogramController::PingChildProcesses() {
  // Only ping ~10% of child processes to avoid possibly "waking up" too many
  // and causing unnecessary work.
  for (const auto& fetcher : renderer_histogram_fetchers_) {
    if (base::RandGenerator(/*range=*/10) == 0) {
      PingChildProcess(fetcher.second.get(),
                       mojom::UmaPingCallSource::PERIODIC);
    }
  }
  for (const auto& fetcher : child_histogram_fetchers_) {
    if (base::RandGenerator(/*range=*/10) == 0) {
      PingChildProcess(fetcher.second.get(),
                       mojom::UmaPingCallSource::PERIODIC);
    }
  }
}

void HistogramController::PingChildProcess(
    content::mojom::ChildHistogramFetcherProxy* fetcher,
    mojom::UmaPingCallSource call_source) {
  // 1) Emit a histogram, 2) ping the child process (which should also emit a
  // histogram), and 3) call Pong(), which again emits a histogram.
  // If no histograms are lost, in total, the histograms should all be emitted
  // roughly the same amount of times. The exception is for 1), which may be
  // emitted more often because this may be called early on in the lifecycle of
  // the child process, and some child processes are killed very early on,
  // before any IPC messages are processed.
  base::UmaHistogramEnumeration(GetPingHistogramName(call_source),
                                mojom::UmaChildPingStatus::BROWSER_SENT_IPC);
  // Unretained is safe because |this| is leaky.
  fetcher->Ping(call_source,
                base::BindOnce(&HistogramController::Pong,
                               base::Unretained(this), call_source));
}

void HistogramController::Pong(mojom::UmaPingCallSource call_source) {
  base::UmaHistogramEnumeration(
      GetPingHistogramName(call_source),
      mojom::UmaChildPingStatus::BROWSER_REPLY_CALLBACK);
}

template <class T>
void HistogramController::RemoveChildHistogramFetcherInterface(
    MayBeDangling<T> host) {
  GetChildHistogramFetcherMap<T>().erase(host);
}

void HistogramController::GetHistogramData(int sequence_number) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  int pending_processes = 0;
  for (RenderProcessHost::iterator it(RenderProcessHost::AllHostsIterator());
       !it.IsAtEnd() && it.GetCurrentValue()->IsReady(); it.Advance()) {
    if (auto* child_histogram_fetcher =
            GetChildHistogramFetcherInterface(it.GetCurrentValue())) {
      child_histogram_fetcher->GetChildNonPersistentHistogramData(
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(&HistogramController::OnHistogramDataCollected,
                             base::Unretained(this), sequence_number),
              std::vector<std::string>()));
      ++pending_processes;
    }
  }

  // TODO(rtenneti): Enable getting histogram data for other processes like
  // PPAPI and NACL.
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    const ChildProcessData& data = iter.GetData();

    // Only get histograms from content process types; skip "embedder" process
    // types.
    if (data.process_type >= PROCESS_TYPE_CONTENT_END)
      continue;

    // In some cases, there may be no child process of the given type (for
    // example, the GPU process may not exist and there may instead just be a
    // GPU thread in the browser process). If that's the case, then the process
    // will be invalid and we shouldn't ask it for data.
    if (!data.GetProcess().IsValid())
      continue;

    if (auto* child_histogram_fetcher =
            GetChildHistogramFetcherInterface(iter.GetHost())) {
      child_histogram_fetcher->GetChildNonPersistentHistogramData(
          mojo::WrapCallbackWithDefaultInvokeIfNotRun(
              base::BindOnce(&HistogramController::OnHistogramDataCollected,
                             base::Unretained(this), sequence_number),
              std::vector<std::string>()));
      ++pending_processes;
    }
  }

  if (subscriber_)
    subscriber_->OnPendingProcesses(sequence_number, pending_processes, true);
}

}  // namespace content
