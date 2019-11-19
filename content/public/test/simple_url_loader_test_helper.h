// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SIMPLE_URL_LOADER_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_SIMPLE_URL_LOADER_TEST_HELPER_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content {

// Test utility class for waiting until a SimpleURLLoader has received a
// response body.
class SimpleURLLoaderTestHelper {
 public:
  SimpleURLLoaderTestHelper();
  ~SimpleURLLoaderTestHelper();

  // Returns a BodyAsStringCallback for use with a SimpleURLLoader. May be
  // called only once.
  network::SimpleURLLoader::BodyAsStringCallback GetCallback();

  // Waits until the callback returned by GetCallback() is invoked.
  void WaitForCallback();

  // Response body passed to the callback returned by GetCallback, if there was
  // one.
  const std::string* response_body() const { return response_body_.get(); }

 private:
  // Called back GetCallback().  Stores the response body and quits the message
  // loop.
  void OnCompleteCallback(std::unique_ptr<std::string> response_body);

  // Used to ensure GetCallback() is called only once.
  bool callback_created_ = false;
  // Used to ensure WaitForCallback() is called only once.
  bool wait_started_ = false;

  base::RunLoop run_loop_;

  std::unique_ptr<std::string> response_body_;

  base::WeakPtrFactory<SimpleURLLoaderTestHelper> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoaderTestHelper);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SIMPLE_URL_LOADER_TEST_HELPER_H_
