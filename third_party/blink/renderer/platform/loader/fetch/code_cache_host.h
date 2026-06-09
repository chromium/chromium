// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_

#include <stdint.h>

#include <memory>

#include "base/containers/heap_array.h"
#include "base/functional/callback.h"
#include "base/functional/function_ref.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/loader/code_cache.mojom-blink-forward.h"
#include "third_party/blink/public/platform/web_common.h"
#include "third_party/blink/renderer/platform/bindings/parkable_string.h"

namespace blink {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
//
// LINT.IfChange(InlineScriptCacheFetchResult)
enum class InlineScriptCacheFetchResult {
  kFetchedNonEmpty = 0,
  kFetchedEmpty = 1,
  kTimedOut = 2,
  kSkippedForSmallScript = 3,
  kMaxValue = kSkippedForSmallScript,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:InlineScriptCacheFetchResult)

// The blink-side interface of `mojom::blink::CodeCacheHost`. This class is a
// thin wrapper around the mojo version except for a mojo-less functionality to
// lookup inline script cache. Note that instances of this class may outlive the
// frame lifetime, e.g., due to teardown ordering.
//
// Important: This class is not allowed to be on the Oilpan heap, since accesses
// to the `mojo::Remote` and the data it holds rely on the object being valid
// (and not poisoned) until the destructor is called.
class BLINK_PLATFORM_EXPORT CodeCacheHost {
 public:
  static std::unique_ptr<CodeCacheHost> Create(
      mojo::Remote<mojom::blink::CodeCacheHost> remote);
  CodeCacheHost(const CodeCacheHost&) = delete;
  CodeCacheHost& operator=(const CodeCacheHost&) = delete;
  virtual ~CodeCacheHost() = default;

  // Fetches an inline script cache entry and returns:
  // (1) an empty `mojo_base::BigBuffer` when cache miss or fetch timed out.
  // (2) a non-empty `mojo_base::BigBuffer` when cache hit.
  //
  // This function is not a mojo wrapper but an endpoint dedicated for renderer.
  // To align with the HTML specification, this function must be blocking;
  // therefore any implementation of this virtual function shall NOT post any
  // task.
  [[nodiscard]] virtual mojo_base::BigBuffer FetchInlineScriptCacheSync(
      const ParkableString& script_source) = 0;

  // Get a weak pointer to this `CodeCacheHost`. Only valid when the remote
  // has been bound.
  virtual base::WeakPtr<CodeCacheHost> GetWeakPtr() = 0;

  virtual mojom::blink::CodeCacheHost* get() = 0;
  virtual mojom::blink::CodeCacheHost& operator*() = 0;
  virtual mojom::blink::CodeCacheHost* operator->() = 0;

 protected:
  class InlineScriptCacheFetcher;

  CodeCacheHost() = default;

  using InlineScriptCacheFetchTrigger = base::FunctionRef<void(
      base::HeapArray<uint8_t> source_hash,
      base::OnceCallback<void(mojo_base::BigBuffer)> callback)>;

  // A thread-safe implementation of fetching inline script on a worker thread
  // while blocking the main thread. `fetch_trigger` is called on the main
  // thread if necessary and should asynchronously start cache fetch on a worker
  // thread When called. Once the fetch operation is completed, the `callback`
  // function of `InlineScriptCacheFetchTrigger` must be called immediately in
  // the worker thread to unblock the main thread; otherwise, the main thread
  // will be kept blocked until the significant, designated time has passed.
  static mojo_base::BigBuffer FetchInlineScriptCacheSyncInternal(
      const ParkableString& script_source,
      InlineScriptCacheFetchTrigger fetch_trigger);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_LOADER_FETCH_CODE_CACHE_HOST_H_
