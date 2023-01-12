// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/crl_set_distributor.h"

#include <string>

#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/observer_list.h"
#include "base/strings/string_piece.h"
#include "base/task/thread_pool.h"
#include "services/network/public/cpp/features.h"

namespace network {

namespace {

// Attempts to parse |crl_set|, returning nullptr on error or the parsed
// CRLSet.
scoped_refptr<net::CRLSet> ParseCRLSet(std::string crl_set) {
  scoped_refptr<net::CRLSet> result;
  if (!net::CRLSet::ParseAndStoreUnparsedData(std::move(crl_set), &result))
    return nullptr;
  return result;
}

// Helper to guarantee |notify_callback| is run, even if |process_callback|
// no-ops due to the worker pool doing the parsing outliving the
// CRLSetDistributor.
void ProcessParsedCRLSet(
    base::OnceCallback<void(scoped_refptr<net::CRLSet>)> process_callback,
    base::OnceClosure notify_callback,
    scoped_refptr<net::CRLSet> crl_set) {
  std::move(process_callback).Run(std::move(crl_set));
  std::move(notify_callback).Run();
}

}  // namespace

CRLSetDistributor::CRLSetDistributor() {}

CRLSetDistributor::~CRLSetDistributor() = default;

void CRLSetDistributor::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void CRLSetDistributor::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void CRLSetDistributor::OnNewCRLSet(base::span<const uint8_t> crl_set,
                                    base::OnceClosure callback) {
  // Make a copy for the background task, since the underlying storage for
  // the span will go away.
  std::string crl_set_string(reinterpret_cast<const char*>(crl_set.data()),
                             crl_set.size());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&ParseCRLSet, std::move(crl_set_string)),
      base::BindOnce(&ProcessParsedCRLSet,
                     base::BindOnce(&CRLSetDistributor::OnCRLSetParsed,
                                    weak_factory_.GetWeakPtr()),
                     std::move(callback)));
}

void CRLSetDistributor::OnCRLSetParsed(scoped_refptr<net::CRLSet> crl_set) {
  if (!crl_set)
    return;  // Error parsing

  if (crl_set_ && crl_set_->sequence() >= crl_set->sequence()) {
    // Don't allow downgrades, and don't refresh CRLSets that are identical
    // (the sequence is globally unique for all CRLSets).
    return;
  }

  crl_set_ = std::move(crl_set);

  for (auto& observer : observers_) {
    observer.OnNewCRLSet(crl_set_);
  }
}

}  // namespace network
