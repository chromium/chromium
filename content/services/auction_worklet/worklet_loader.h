// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_

#include <stddef.h>

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/auction_worklet_service_impl.h"
#include "content/services/auction_worklet/public/cpp/auction_downloader.h"
#include "content/services/auction_worklet/public/cpp/auction_network_events_delegate.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/gurl.h"
#include "v8/include/v8-persistent-handle.h"

namespace v8 {
class UnboundScript;
class WasmModuleObject;
}  // namespace v8

namespace auction_worklet {

class AuctionDownloader;

// Helper for downloading things and compiling them with V8 into an
// UnboundScript or WasmModuleObject on the V8 thread. Create via the
// appropriate subclass. That also provides the way extracting the appropriate
// type from Result.
class CONTENT_EXPORT WorkletLoaderBase {
 public:
  // The result of loading JS or Wasm, memory-managing the underlying V8 object.
  //
  // This helps ensure that the script handle is deleted on the right thread
  // even in case when the callback handling the result is destroyed.
  class CONTENT_EXPORT Result {
   public:
    Result();
    Result(Result&&);
    ~Result();

    Result& operator=(Result&&);

    // True if the script or module was loaded & compiled successfully.
    // Only meaningful after the user-callback is invoked; before that things
    // may not be filled in yet.
    bool success() const { return success_; }

    size_t original_size_bytes() const { return original_size_bytes_; }
    base::TimeDelta download_time() const { return download_time_; }

   private:
    friend class WorkletLoader;
    friend class WorkletLoaderBase;
    friend class WorkletWasmLoader;

    // This creates the V8 state object and fills in the main-thread-side
    // metrics on the download. Afterwards `state_` exists.
    void DownloadReady(scoped_refptr<AuctionV8Helper> v8_helper,
                       size_t original_size_bytes,
                       base::TimeDelta download_time);

    void set_success(bool success) { success_ = success; }

    // Will be deleted on v8_helper_->v8_runner(). See https://crbug.com/1231690
    // for why this is structured this way.
    struct V8Data {
      explicit V8Data(scoped_refptr<AuctionV8Helper> v8_helper);
      ~V8Data();

      void SetScript(v8::Global<v8::UnboundScript> script);
      void SetModule(v8::Global<v8::WasmModuleObject> wasm_module);

      bool compiled = false;

      scoped_refptr<AuctionV8Helper> v8_helper;
      // These start empty, are filled in once download is parsed on v8 thread.
      // TakeScript() can clear `script`.
      v8::Global<v8::UnboundScript> script;
      v8::Global<v8::WasmModuleObject> wasm_module;
    };

    std::unique_ptr<V8Data, base::OnTaskRunnerDeleter> state_;

    // Used only for metrics; the original size of the uncompiled JS or WASM
    // body.
    size_t original_size_bytes_ = 0;
    // Used only for metrics; the time required to download.
    base::TimeDelta download_time_;

    bool success_ = false;
  };

  using LoadWorkletCallback =
      base::OnceCallback<void(std::vector<Result> results,
                              std::optional<std::string> error_msg)>;

  explicit WorkletLoaderBase(const WorkletLoaderBase&) = delete;
  WorkletLoaderBase& operator=(const WorkletLoaderBase&) = delete;

 protected:
  // Starts loading the resource on construction. The same resource will be
  // loaded in each of the threads associated with `v8_helpers` and `debug_ids`.
  // Callback will be invoked on the current thread asynchronously once the data
  // has been fetched and compiled on all V8 threads, or an error has occurred.
  // Destroying this is guaranteed to cancel the callback. `mime_type` will
  // inform both download checking and the compilation method used.
  WorkletLoaderBase(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& source_url,
      AuctionDownloader::MimeType mime_type,
      std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
      std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids,
      AuctionDownloader::ResponseStartedCallback response_started_callback,
      LoadWorkletCallback load_worklet_callback);
  ~WorkletLoaderBase();

 private:
  void OnDownloadComplete(std::unique_ptr<std::string> body,
                          scoped_refptr<net::HttpResponseHeaders> headers,
                          std::optional<std::string> error_msg);

  static void HandleDownloadResultOnV8Thread(
      GURL source_url,
      AuctionDownloader::MimeType mime_type,
      scoped_refptr<AuctionV8Helper> v8_helper,
      scoped_refptr<AuctionV8Helper::DebugId> debug_id,
      std::unique_ptr<std::string> body,
      std::optional<std::string> error_msg,
      WorkletLoaderBase::Result::V8Data* out_data,
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<WorkletLoaderBase> weak_instance);

  static bool CompileJs(const std::string& body,
                        scoped_refptr<AuctionV8Helper> v8_helper,
                        const GURL& source_url,
                        AuctionV8Helper::DebugId* debug_id,
                        std::optional<std::string>& error_msg,
                        WorkletLoaderBase::Result::V8Data* out_data);

  static bool CompileWasm(const std::string& body,
                          scoped_refptr<AuctionV8Helper> v8_helper,
                          const GURL& source_url,
                          AuctionV8Helper::DebugId* debug_id,
                          std::optional<std::string>& error_msg,
                          WorkletLoaderBase::Result::V8Data* out_data);

  void DeliverCallbackOnUserThread(bool success,
                                   std::optional<std::string> error_msg,
                                   bool download_success);

  const GURL source_url_;
  const AuctionDownloader::MimeType mime_type_;
  const std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers_;
  const std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids_;
  const base::TimeTicks start_time_;

  size_t response_received_count_ = 0;

  // We manage the results here until it's handed to the client, or we are
  // destroyed. The second case lets us clean up the V8 state w/o waiting for
  // main event loop to cleanup callbacks from V8 thread -> main, which can be
  // tricky at shutdown.
  //
  // See https://crbug.com/1421754
  std::vector<Result> pending_results_;

  std::unique_ptr<AuctionDownloader> auction_downloader_;
  LoadWorkletCallback load_worklet_callback_;

  base::WeakPtrFactory<WorkletLoaderBase> weak_ptr_factory_{this};
};

// Utility for loading and compiling worklet JavaScript.
class CONTENT_EXPORT WorkletLoader : public WorkletLoaderBase {
 public:
  using AllowTrustedScoringSignalsCallback =
      base::OnceCallback<void(std::vector<url::Origin>)>;

  // Starts loading the resource on construction. The same resource will be
  // loaded in each of the threads associated with `v8_helpers`.
  //
  // `load_worklet_callback` will be invoked on the current thread
  // asynchronously once the data has been fetched and compiled on all V8
  // threads, or an error has occurred.
  //
  // If `allow_trusted_scoring_signals_callback` is specified, it will
  // be invoked, on the current thread, with parsed contents of
  // Ad-Auction-Allow-Trusted-Scoring-Signals-From header as soon as headers
  // are available, unless the headers were enough for the request to be
  // rejected.
  //
  // Destroying this is guaranteed to cancel any outstanding callbacks.
  WorkletLoader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& source_url,
      std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
      std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids,
      AllowTrustedScoringSignalsCallback allow_trusted_scoring_signals_callback,
      LoadWorkletCallback load_worklet_callback);
  ~WorkletLoader();

  // The returned value is a compiled script not bound to any context. It
  // can be repeatedly bound to different contexts and executed, without
  // persisting any state.  `result` will be cleared after this call.
  //
  // Should only be called on the V8 thread. Requires `result.success()` to be
  // true.
  static v8::Global<v8::UnboundScript> TakeScript(Result&& result);

  // Parses a header describing which origins may provide trusted seller
  // signals. It is expected to be a structured header list of strings,
  // each of each is an https:// URL denoting the corresponding origin.
  //
  // Public for testing.
  static std::vector<url::Origin> ParseAllowTrustedScoringSignalsFromHeader(
      const std::string& allow_trusted_scoring_signals_from_header);

 private:
  void OnResponseStarted(const network::mojom::URLResponseHead&);

  AllowTrustedScoringSignalsCallback allow_trusted_scoring_signals_callback_;
};

class CONTENT_EXPORT WorkletWasmLoader : public WorkletLoaderBase {
 public:
  // Starts loading the resource on construction. The same resource will be
  // loaded in each of the threads associated with `v8_helpers`. Callback will
  // be invoked on the current thread asynchronously once the data has been
  // fetched and compiled on all V8 threads, or an error has occurred.
  // Destroying this is guaranteed to cancel the callback.
  WorkletWasmLoader(
      network::mojom::URLLoaderFactory* url_loader_factory,
      mojo::PendingRemote<auction_worklet::mojom::AuctionNetworkEventsHandler>
          auction_network_events_handler,
      const GURL& source_url,
      std::vector<scoped_refptr<AuctionV8Helper>> v8_helpers,
      std::vector<scoped_refptr<AuctionV8Helper::DebugId>> debug_ids,
      LoadWorkletCallback load_worklet_callback);

  // The returned value is a module object. Since it's a JS object, it
  // should not be shared between contexts that must be isolated, as the code
  // can just set properties on it. Instead, create a new instance using
  // MakeModule() every time. `result` will not be changed by this call.
  //
  // This should only be called on the V8 thread, with a context active.
  // Requires `result.success()` to be true.
  static v8::MaybeLocal<v8::WasmModuleObject> MakeModule(const Result& result);
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
