// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_METRICS_HISTOGRAM_CONTROLLER_H_
#define CONTENT_BROWSER_METRICS_HISTOGRAM_CONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/timer/timer.h"
#include "content/common/histogram_fetcher.mojom-shared.h"
#include "content/common/histogram_fetcher.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace content {

class HistogramSubscriber;
class ChildProcessHost;
class RenderProcessHost;

// HistogramController is used on the browser process to collect histogram data.
// Only the browser UI thread is allowed to interact with the
// HistogramController object.
class HistogramController {
 public:
  // Returns the HistogramController object for the current process, or NULL if
  // none.
  static HistogramController* GetInstance();

  // Normally instantiated when the child process is launched. Only one instance
  // should be created per process.
  HistogramController();

  HistogramController(const HistogramController&) = delete;
  HistogramController& operator=(const HistogramController&) = delete;

  virtual ~HistogramController();

  // Register the subscriber so that it will be called when for example
  // OnHistogramDataCollected is returning histogram data from a child process.
  // This is called on UI thread.
  void Register(HistogramSubscriber* subscriber);

  // Unregister the subscriber so that it will not be called when for example
  // OnHistogramDataCollected is returning histogram data from a child process.
  // Safe to call even if caller is not the current subscriber.
  void Unregister(const HistogramSubscriber* subscriber);

  // Contact all processes and get their histogram data.
  void GetHistogramData(int sequence_number);

  // Send the |histogram| back to the |subscriber_|.
  // This can be called from any thread.
  void OnHistogramDataCollected(
      int sequence_number,
      const std::vector<std::string>& pickled_histograms);

  template <class T>
  void SetHistogramMemory(T*, base::WritableSharedMemoryRegion);

  // Some hosts can be re-used before Mojo recognizes that their connections
  // are invalid because the previous child process died.
  template <class T>
  void NotifyChildDied(T*);

 private:
  friend struct base::LeakySingletonTraits<HistogramController>;

  raw_ptr<HistogramSubscriber> subscriber_;

  template <class T>
  using ChildHistogramFetcherMap =
      std::map<T*, mojo::Remote<content::mojom::ChildHistogramFetcher>>;

  template <class T>
  void InsertChildHistogramFetcherInterface(
      T* host,
      mojo::Remote<content::mojom::ChildHistogramFetcher>
          child_histogram_fetcher);

  template <class T>
  content::mojom::ChildHistogramFetcher* GetChildHistogramFetcherInterface(
      T* host);

  // Calls PingChildProcess() on ~10% of child processes. Not all child
  // processes are pinged so as to avoid possibly "waking up" too many and
  // causing unnecessary work.
  void PingChildProcesses();

  // Pings a child process through its |fetcher|. This does nothing except emit
  // histograms (both on the browser process and the child process), with the
  // goal of quantifying the amount of histogram samples lost from child
  // processes.
  void PingChildProcess(content::mojom::ChildHistogramFetcherProxy* fetcher,
                        mojom::UmaPingCallSource call_source);

  // Callback for when a child process has received a ping (see
  // PingChildProcess()).
  void Pong(mojom::UmaPingCallSource call_source);

  template <class T>
  void RemoveChildHistogramFetcherInterface(MayBeDangling<T> host);

  // Specialize this template for each ChildHistogramFetcherMap defined below.
  template <class T>
  ChildHistogramFetcherMap<T>& GetChildHistogramFetcherMap();

  ChildHistogramFetcherMap<RenderProcessHost> renderer_histogram_fetchers_;
  ChildHistogramFetcherMap<ChildProcessHost> child_histogram_fetchers_;

  // Used to call PingAllChildProcesses() every 5 minutes.
  base::RepeatingTimer timer_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_METRICS_HISTOGRAM_CONTROLLER_H_
