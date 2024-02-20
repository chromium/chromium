/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_url_request_extra_data.h"
#include "base/memory/raw_ptr.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

class RequestTestExtraData : public WebURLRequestExtraData {
 public:
  explicit RequestTestExtraData(bool* alive) : alive_(alive) { *alive = true; }

 private:
  ~RequestTestExtraData() override { *alive_ = false; }

  raw_ptr<bool> alive_;
};

}  // anonymous namespace

TEST(WebURLRequestExtraDataTest, ExtraData) {
  test::TaskEnvironment task_environment;
  bool alive = false;
  {
    WebURLRequest url_request;
    auto url_request_extra_data =
        base::MakeRefCounted<RequestTestExtraData>(&alive);
    EXPECT_TRUE(alive);

    auto* raw_request_extra_data_pointer = url_request_extra_data.get();
    url_request.SetURLRequestExtraData(std::move(url_request_extra_data));
    EXPECT_EQ(raw_request_extra_data_pointer,
              url_request.GetURLRequestExtraData());
    {
      WebURLRequest other_url_request;
      other_url_request.CopyFrom(url_request);
      EXPECT_TRUE(alive);
      EXPECT_EQ(raw_request_extra_data_pointer,
                other_url_request.GetURLRequestExtraData());
      EXPECT_EQ(raw_request_extra_data_pointer,
                url_request.GetURLRequestExtraData());
    }
    EXPECT_TRUE(alive);
    EXPECT_EQ(raw_request_extra_data_pointer,
              url_request.GetURLRequestExtraData());
  }
  EXPECT_FALSE(alive);
}

}  // namespace blink
