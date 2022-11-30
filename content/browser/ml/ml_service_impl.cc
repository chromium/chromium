// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/ml/ml_service_impl.h"

#include "base/memory/ptr_util.h"
#include "components/ml/mojom/ml_service.mojom.h"
#include "components/ml/mojom/web_platform_model.mojom.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"

namespace content {

namespace {

using ml::model_loader::mojom::CreateModelLoaderOptionsPtr;
using ml::model_loader::mojom::CreateModelLoaderResult;
using ml::model_loader::mojom::MLService;

}  // namespace

// static
void MLServiceImpl::Create(mojo::PendingReceiver<MLService> receiver) {
  mojo::MakeSelfOwnedReceiver<MLService>(base::WrapUnique(new MLServiceImpl()),
                                         std::move(receiver));
}

MLServiceImpl::~MLServiceImpl() = default;

MLServiceImpl::MLServiceImpl() = default;

void MLServiceImpl::CreateModelLoader(CreateModelLoaderOptionsPtr options,
                                      CreateModelLoaderCallback callback) {
  // TODO(https://crbug.com/1309672): We should consider supporting this API on
  // the other platforms in the future.
  std::move(callback).Run(CreateModelLoaderResult::kNotSupported,
                          mojo::NullRemote());
}

}  // namespace content
