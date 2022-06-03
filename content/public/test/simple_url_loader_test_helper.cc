// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/simple_url_loader_test_helper.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/check.h"

namespace content {

SimpleURLLoaderTestHelper::SimpleURLLoaderTestHelper() {}

SimpleURLLoaderTestHelper::~SimpleURLLoaderTestHelper() {}

network::SimpleURLLoader::BodyAsStringCallback
SimpleURLLoaderTestHelper::GetCallback() {
  DCHECK(!callback_created_);
  callback_created_ = true;

  return base::BindOnce(&SimpleURLLoaderTestHelper::OnCompleteCallback,
                        weak_ptr_factory_.GetWeakPtr());
}

void SimpleURLLoaderTestHelper::WaitForCallback() {
  DCHECK(!wait_started_);
  wait_started_ = true;
  run_loop_.Run();
}

void SimpleURLLoaderTestHelper::OnCompleteCallback(
    std::unique_ptr<std::string> response_body) {
  DCHECK(!response_body_);

  response_body_ = std::move(response_body);

  run_loop_.Quit();
}

}  // namespace content
