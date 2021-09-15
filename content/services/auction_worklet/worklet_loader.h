// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
#define CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "url/gurl.h"
#include "v8/include/v8-persistent-handle.h"

namespace v8 {
class UnboundScript;
}  // namespace v8

namespace auction_worklet {

class AuctionDownloader;

// Utility class to download and build a worklet.
class WorkletLoader {
 public:
  // This helps ensure that the script handle is deleted on the right thread
  // even in case when the callback handling the result is destroyed.
  class Result {
   public:
    Result();
    Result(scoped_refptr<AuctionV8Helper> v8_helper,
           v8::Global<v8::UnboundScript>);
    Result(Result&&);
    ~Result();

    Result& operator=(Result&&);

    // True if the script was loaded & parsed successfully.
    bool success() const { return state_.get() && !state_->script.IsEmpty(); }

    // Should only be called on the V8 thread. Requires success() to be true.
    v8::Global<v8::UnboundScript> TakeScript();

   private:
    // Will be deleted on v8_helper_->v8_runner(). See https://crbug.com/1231690
    // for why this is structured this way.
    struct V8Data {
      V8Data(scoped_refptr<AuctionV8Helper> v8_helper,
             v8::Global<v8::UnboundScript> script);
      ~V8Data();

      scoped_refptr<AuctionV8Helper> v8_helper;
      v8::Global<v8::UnboundScript> script;
    };

    std::unique_ptr<V8Data, base::OnTaskRunnerDeleter> state_;
  };

  // On success, `worklet_script` is compiled script, not bound to any context.
  // It can be repeatedly bound to different contexts and executed, without
  // persisting any state.
  using LoadWorkletCallback =
      base::OnceCallback<void(Result worklet_script,
                              absl::optional<std::string> error_msg)>;

  // Starts loading the worklet script on construction. Callback will be invoked
  // asynchronously once the data has been fetched or an error has occurred, on
  // the current thread. Destroying this is guaranteed to cancel the callback.
  WorkletLoader(network::mojom::URLLoaderFactory* url_loader_factory,
                const GURL& script_source_url,
                scoped_refptr<AuctionV8Helper> v8_helper,
                int debug_context_group_id,
                LoadWorkletCallback load_worklet_callback);
  explicit WorkletLoader(const WorkletLoader&) = delete;
  WorkletLoader& operator=(const WorkletLoader&) = delete;
  ~WorkletLoader();

 private:
  void OnDownloadComplete(std::unique_ptr<std::string> body,
                          absl::optional<std::string> error_msg);

  static void HandleDownloadResultOnV8Thread(
      GURL script_source_url,
      scoped_refptr<AuctionV8Helper> v8_helper,
      int debug_context_group_id,
      std::unique_ptr<std::string> body,
      absl::optional<std::string> error_msg,
      scoped_refptr<base::SequencedTaskRunner> user_thread_task_runner,
      base::WeakPtr<WorkletLoader> weak_instance);

  void DeliverCallbackOnUserThread(Result worklet_script,
                                   absl::optional<std::string> error_msg);

  const GURL script_source_url_;
  const scoped_refptr<AuctionV8Helper> v8_helper_;
  int debug_context_group_id_;

  std::unique_ptr<AuctionDownloader> auction_downloader_;
  LoadWorkletCallback load_worklet_callback_;

  base::WeakPtrFactory<WorkletLoader> weak_ptr_factory_{this};
};

}  // namespace auction_worklet

#endif  // CONTENT_SERVICES_AUCTION_WORKLET_WORKLET_LOADER_H_
