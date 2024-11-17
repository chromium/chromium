// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/services/auction_worklet/trusted_signals_kvv2_manager.h"

#include <map>
#include <memory>
#include <optional>
#include <string>

#include "base/cancelable_callback.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/types/expected.h"
#include "base/types/optional_ref.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "content/common/content_export.h"
#include "content/common/features.h"
#include "content/services/auction_worklet/auction_v8_helper.h"
#include "content/services/auction_worklet/public/mojom/trusted_signals_cache.mojom.h"
#include "content/services/auction_worklet/trusted_signals.h"
#include "content/services/auction_worklet/trusted_signals_kvv2_helper.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/zlib/google/compression_utils.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "v8/include/v8-context.h"

namespace auction_worklet {

namespace {

// Tries to decompress and fully parse `compression_group_data` and returns the
// result. May only be called on the V8 thread.
TrustedSignalsKVv2Manager::PartitionMapOrError ParseCompressionGroupOnV8Thread(
    AuctionV8Helper* v8_helper,
    TrustedSignalsKVv2Manager::SignalsType signals_type,
    mojom::TrustedSignalsCompressionScheme compression_scheme,
    mojo_base::BigBuffer compression_group_data) {
  DCHECK(v8_helper->v8_runner()->RunsTasksInCurrentSequence());

  AuctionV8Helper::FullIsolateScope isolate_scope(v8_helper);
  v8::Context::Scope context_scope(v8_helper->scratch_context());
  return TrustedSignalsKVv2ResponseParser::ParseEntireCompressionGroup(
      v8_helper, signals_type, compression_scheme,
      base::span(compression_group_data));
}

}  // namespace

class TrustedSignalsKVv2Manager::RequestImpl : public Request {
 public:
  explicit RequestImpl(TrustedSignalsKVv2Manager* manager,
                       int partition_id,
                       CompressionGroupMap::iterator compression_group_it,
                       LoadSignalsCallback load_signals_callback)
      : manager_(manager),
        partition_id_(partition_id),
        compression_group_it_(compression_group_it),
        load_signals_callback_(std::move(load_signals_callback)) {}

  ~RequestImpl() override {
    manager_->OnRequestDestroyed(this, compression_group_it_);
  }

  int partition_id() const { return partition_id_; }

  void RunCallbackAsynchronously(ResultOrError result) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&RequestImpl::RunCallback,
                       weak_ptr_factory_.GetWeakPtr(), std::move(result)));
  }

 private:
  void RunCallback(ResultOrError result) {
    if (result.has_value()) {
      std::move(load_signals_callback_)
          .Run(/*signals=*/std::move(result).value(),
               /*error_msg=*/std::nullopt);
    } else {
      std::move(load_signals_callback_)
          .Run(/*signals=*/nullptr, /*error_msg=*/std::move(result).error());
    }
  }

  const raw_ptr<TrustedSignalsKVv2Manager> manager_;

  const int partition_id_;
  const CompressionGroupMap::iterator compression_group_it_;
  LoadSignalsCallback load_signals_callback_;

  base::WeakPtrFactory<RequestImpl> weak_ptr_factory_{this};
};

struct TrustedSignalsKVv2Manager::CompressionGroup {
  explicit CompressionGroup(SignalsType signals_type)
      : signals_type(signals_type) {}

  SignalsType signals_type;

  // The Requests this is associated with. Before `parse_result` is populated,
  // these requests have yet to have their callbacks invoked. Once the result
  // has been received, these have all had their callbacks invoked, or have a
  // pending task to invoke them asynchronously.
  std::set<raw_ptr<RequestImpl, SetExperimental>> requests;

  // A receiver while the CompressionGroup is waiting on a response from the
  // browser. Null when there's no associated receiver.
  std::optional<mojo::ReceiverId> receiver_id;

  // Populated on completion, regardless of success or failure.
  std::optional<PartitionMapOrError> parse_result;
};

TrustedSignalsKVv2Manager::TrustedSignalsKVv2Manager(
    mojo::PendingRemote<mojom::TrustedSignalsCache> trusted_signals_cache,
    scoped_refptr<AuctionV8Helper> v8_helper)
    : trusted_signals_cache_(std::move(trusted_signals_cache)),
      v8_helper_(std::move(v8_helper)) {}

TrustedSignalsKVv2Manager::~TrustedSignalsKVv2Manager() = default;

std::unique_ptr<TrustedSignalsKVv2Manager::Request>
TrustedSignalsKVv2Manager::RequestSignals(
    SignalsType signals_type,
    base::UnguessableToken compression_group_token,
    int partition_id,
    LoadSignalsCallback load_signals_callback) {
  auto [compression_group_it, created_new_group] =
      compression_groups_.try_emplace(compression_group_token, signals_type);

  // Start fetching compression group, if matching group was found.
  if (created_new_group) {
    mojo::PendingReceiver<mojom::TrustedSignalsCacheClient> pending_receiver;
    trusted_signals_cache_->GetTrustedSignals(
        compression_group_token,
        pending_receiver.InitWithNewPipeAndPassRemote());
    // This does doesn't bother to watch for pipe errors. The browser process
    // does not close the pipes without sending a response execept on teardown
    // or crash, and in either case, this process will be torn down as well.
    compression_group_it->second.receiver_id = compression_group_pipes_.Add(
        this, std::move(pending_receiver), compression_group_it);
  }

  // The same compression group should never be requested with different signals
  // types.
  CHECK_EQ(compression_group_it->second.signals_type, signals_type);

  auto request =
      std::make_unique<RequestImpl>(this, partition_id, compression_group_it,
                                    std::move(load_signals_callback));
  auto& compression_group = compression_group_it->second;
  compression_group.requests.insert(request.get());

  // If the compression group has already has a response, post a task to invoke
  // the callback asynchronously. Doing it synchronously would be problematic,
  // as RequestImpl hasn't been returned yet.
  if (compression_group.parse_result) {
    request->RunCallbackAsynchronously(
        GetResultForPartition(compression_group, partition_id));
  }

  return request;
}

void TrustedSignalsKVv2Manager::OnSuccess(
    mojom::TrustedSignalsCompressionScheme compression_scheme,
    mojo_base::BigBuffer compression_group_data) {
  v8_helper_->v8_runner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(
          &ParseCompressionGroupOnV8Thread, base::RetainedRef(v8_helper_),
          compression_group_pipes_.current_context()->second.signals_type,
          compression_scheme, std::move(compression_group_data)),
      base::BindOnce(&TrustedSignalsKVv2Manager::OnComplete,
                     weak_ptr_factory_.GetWeakPtr(),
                     compression_group_pipes_.current_context()->first));
  // Closing the pipe destroys the pipe's current context, so is only safe to do
  // after we're done pulling data from the context.
  ClosePipe(compression_group_pipes_.current_context());
}

void TrustedSignalsKVv2Manager::OnError(const std::string& error_message) {
  OnComplete(compression_group_pipes_.current_context()->first,
             base::unexpected(error_message));
  // Closing the pipe destroys the pipe's current context, so is only safe to do
  // after we're done pulling data from the context.
  ClosePipe(compression_group_pipes_.current_context());
}

void TrustedSignalsKVv2Manager::OnComplete(
    base::UnguessableToken compression_group_token,
    PartitionMapOrError parsed_compression_group_result) {
  // It's possible all requests associated with the compression group were
  // cancelled while work was being done on the V8 thread.
  auto compression_group_it = compression_groups_.find(compression_group_token);
  if (compression_group_it == compression_groups_.end()) {
    return;
  }
  CompressionGroup* compression_group = &compression_group_it->second;
  compression_group->parse_result = std::move(parsed_compression_group_result);

  for (auto request : compression_group->requests) {
    // Run callbacks asynchronously, since they could delete the request, which
    // calls back into `this`.
    request->RunCallbackAsynchronously(
        GetResultForPartition(*compression_group, request->partition_id()));
  }
}

void TrustedSignalsKVv2Manager::OnRequestDestroyed(
    RequestImpl* request,
    CompressionGroupMap::iterator compression_group_it) {
  compression_group_it->second.requests.erase(request);

  // Destroy the compression group if there are no more requests using it. This
  // results in very basic caching behavior, if consumers hold onto their
  // RequestImpls until after they're done with the resulting Signals objects.
  if (compression_group_it->second.requests.empty()) {
    if (compression_group_it->second.receiver_id) {
      ClosePipe(compression_group_it);
    }
    compression_groups_.erase(compression_group_it);
  }
}

void TrustedSignalsKVv2Manager::ClosePipe(
    CompressionGroupMap::iterator compression_group_it) {
  DCHECK(compression_group_it->second.receiver_id);
  compression_group_pipes_.Remove(*compression_group_it->second.receiver_id);
  compression_group_it->second.receiver_id = std::nullopt;
}

TrustedSignalsKVv2Manager::ResultOrError
TrustedSignalsKVv2Manager::GetResultForPartition(
    const CompressionGroup& compression_group,
    int partition_id) {
  DCHECK(compression_group.parse_result);
  if (!compression_group.parse_result->has_value()) {
    return base::unexpected(compression_group.parse_result->error());
  }

  auto partition_it = (**compression_group.parse_result).find(partition_id);
  if (partition_it == (**compression_group.parse_result).end()) {
    return base::unexpected(base::StringPrintf(
        R"(Partition "%i" is missing from response.)", partition_id));
  }

  return partition_it->second;
}

}  // namespace auction_worklet
