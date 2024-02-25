// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/simple_url_loader_test_helper.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

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

network::SimpleURLLoader::BodyAsStringCallbackDeprecated
SimpleURLLoaderTestHelper::GetCallbackDeprecated() {
  DCHECK(!callback_created_);
  callback_created_ = true;

  return base::BindOnce(
      &SimpleURLLoaderTestHelper::OnCompleteCallbackDeprecated,
      weak_ptr_factory_.GetWeakPtr());
}

void SimpleURLLoaderTestHelper::WaitForCallback() {
  DCHECK(!wait_started_);
  wait_started_ = true;
  run_loop_.Run();
}

void SimpleURLLoaderTestHelper::OnCompleteCallbackDeprecated(
    std::unique_ptr<std::string> response_body) {
  DCHECK(!response_body_);

  if (response_body) {
    response_body_ = std::move(*response_body);
  }

  run_loop_.Quit();
}

void SimpleURLLoaderTestHelper::OnCompleteCallback(
    std::optional<std::string> response_body) {
  DCHECK(!response_body_);

  response_body_ = std::move(response_body);

  run_loop_.Quit();
}

}  // namespace content
