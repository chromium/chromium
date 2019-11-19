// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_HISTOGRAM_FETCHER_IMPL_H
#define CONTENT_CHILD_CHILD_HISTOGRAM_FETCHER_IMPL_H

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/writable_shared_memory_region.h"
#include "content/common/histogram_fetcher.mojom.h"
#include "ipc/message_filter.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace base {
class HistogramDeltaSerialization;
}  // namespace base

namespace content {

class ChildHistogramFetcherFactoryImpl
    : public content::mojom::ChildHistogramFetcherFactory {
 public:
  ChildHistogramFetcherFactoryImpl();
  ~ChildHistogramFetcherFactoryImpl() override;

  static void Create(
      mojo::PendingReceiver<content::mojom::ChildHistogramFetcherFactory>);

 private:
  void CreateFetcher(
      base::WritableSharedMemoryRegion,
      mojo::PendingReceiver<content::mojom::ChildHistogramFetcher>) override;
};

class ChildHistogramFetcherImpl : public content::mojom::ChildHistogramFetcher {
 public:
  ChildHistogramFetcherImpl();
  ~ChildHistogramFetcherImpl() override;

 private:
  typedef std::vector<std::string> HistogramPickledList;

  // content::mojom::ChildHistogram implementation.
  using HistogramDataCallback = content::mojom::ChildHistogramFetcher::
      GetChildNonPersistentHistogramDataCallback;

  void GetChildNonPersistentHistogramData(
      HistogramDataCallback callback) override;

  // Extract snapshot data and then send it off to the Browser process.
  // Send only a delta to what we have already sent.
  void UploadAllHistograms(int64_t sequence_number);

  // Prepares histogram deltas for transmission.
  std::unique_ptr<base::HistogramDeltaSerialization>
      histogram_delta_serialization_;

  DISALLOW_COPY_AND_ASSIGN(ChildHistogramFetcherImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_HISTOGRAM_FETCHER_IMPL_H
