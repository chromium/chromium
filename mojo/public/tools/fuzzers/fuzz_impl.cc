// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/tools/fuzzers/fuzz.mojom.h"
#include "mojo/public/tools/fuzzers/fuzz_impl.h"

FuzzImpl::FuzzImpl(mojo::PendingReceiver<fuzz::mojom::FuzzInterface> receiver)
    : receiver_(this, std::move(receiver)) {}

FuzzImpl::~FuzzImpl() {}

void FuzzImpl::FuzzBasic() {}

void FuzzImpl::FuzzBasicResp(FuzzBasicRespCallback callback) {
  std::move(callback).Run();
}

void FuzzImpl::FuzzBasicSyncResp(FuzzBasicSyncRespCallback callback) {
  std::move(callback).Run();
}

void FuzzImpl::FuzzArgs(fuzz::mojom::FuzzStructPtr a,
                        fuzz::mojom::FuzzStructPtr b) {}

void FuzzImpl::FuzzArgsResp(fuzz::mojom::FuzzStructPtr a,
                            fuzz::mojom::FuzzStructPtr b,
                            FuzzArgsRespCallback callback) {
  std::move(callback).Run();
}

void FuzzImpl::FuzzArgsSyncResp(fuzz::mojom::FuzzStructPtr a,
                                fuzz::mojom::FuzzStructPtr b,
                                FuzzArgsSyncRespCallback callback) {
  std::move(callback).Run();
}

void FuzzImpl::FuzzAssociated(
    mojo::PendingAssociatedReceiver<fuzz::mojom::FuzzDummyInterface> receiver) {
  associated_receivers_.Add(this, std::move(receiver));
}

void FuzzImpl::Ping() {}
