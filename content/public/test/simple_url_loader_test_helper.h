// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_SIMPLE_URL_LOADER_TEST_HELPER_H_
#define CONTENT_PUBLIC_TEST_SIMPLE_URL_LOADER_TEST_HELPER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "services/network/public/cpp/simple_url_loader.h"

namespace content {

// Test utility class for waiting until a SimpleURLLoader has received a
// response body.
class SimpleURLLoaderTestHelper {
 public:
  SimpleURLLoaderTestHelper();

  SimpleURLLoaderTestHelper(const SimpleURLLoaderTestHelper&) = delete;
  SimpleURLLoaderTestHelper& operator=(const SimpleURLLoaderTestHelper&) =
      delete;

  ~SimpleURLLoaderTestHelper();

  // Returns a BodyAsStringCallbackDeprecated for use with a SimpleURLLoader.
  // May be called only once.
  network::SimpleURLLoader::BodyAsStringCallback GetCallback();
  network::SimpleURLLoader::BodyAsStringCallbackDeprecated
  GetCallbackDeprecated();

  // Waits until the callback returned by GetCallback() is invoked.
  void WaitForCallback();

  // Response body passed to the callback returned by GetCallback, if there was
  // one.
  const std::optional<std::string>& response_body() const {
    return response_body_;
  }

 private:
  // Called back GetCallback().  Stores the response body and quits the message
  // loop.
  void OnCompleteCallback(std::optional<std::string> response_body);
  void OnCompleteCallbackDeprecated(std::unique_ptr<std::string> response_body);

  // Used to ensure GetCallback() is called only once.
  bool callback_created_ = false;
  // Used to ensure WaitForCallback() is called only once.
  bool wait_started_ = false;

  base::RunLoop run_loop_;

  std::optional<std::string> response_body_;

  base::WeakPtrFactory<SimpleURLLoaderTestHelper> weak_ptr_factory_{this};
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_SIMPLE_URL_LOADER_TEST_HELPER_H_
