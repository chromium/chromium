// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/mojom/url_test.mojom-blink.h"
#include "url/url_constants.h"

namespace blink {
namespace {

class UrlTestImpl : public url::mojom::blink::UrlTest {
 public:
  explicit UrlTestImpl(
      mojo::PendingReceiver<url::mojom::blink::UrlTest> receiver)
      : receiver_(this, std::move(receiver)) {}

  // UrlTest:
  void BounceUrl(const KURL& in, BounceUrlCallback callback) override {
    std::move(callback).Run(in);
  }

  void BounceOrigin(const scoped_refptr<const SecurityOrigin>& in,
                    BounceOriginCallback callback) override {
    std::move(callback).Run(in);
  }

 private:
  mojo::Receiver<UrlTest> receiver_;
};

}  // namespace

// Mojo version of chrome IPC test in url/ipc/url_param_traits_unittest.cc.
TEST(KURLSecurityOriginStructTraitsTest, Basic) {
  base::test::TaskEnvironment task_environment;

  mojo::Remote<url::mojom::blink::UrlTest> remote;
  UrlTestImpl impl(remote.BindNewPipeAndPassReceiver());

  const char* serialize_cases[] = {
      "http://www.google.com/", "http://user:pass@host.com:888/foo;bar?baz#nop",
  };

  for (const char* test_case : serialize_cases) {
    KURL input(NullURL(), test_case);
    KURL output;
    EXPECT_TRUE(remote->BounceUrl(input, &output));

    // We want to test each component individually to make sure its range was
    // correctly serialized and deserialized, not just the spec.
    EXPECT_EQ(input.GetString(), output.GetString());
    EXPECT_EQ(input.IsValid(), output.IsValid());
    EXPECT_EQ(input.Protocol(), output.Protocol());
    EXPECT_EQ(input.User(), output.User());
    EXPECT_EQ(input.Pass(), output.Pass());
    EXPECT_EQ(input.Host(), output.Host());
    EXPECT_EQ(input.Port(), output.Port());
    EXPECT_EQ(input.GetPath(), output.GetPath());
    EXPECT_EQ(input.Query(), output.Query());
    EXPECT_EQ(input.FragmentIdentifier(), output.FragmentIdentifier());
  }

  // Test an excessively long GURL.
  {
    const std::string url =
        std::string("http://example.org/").append(url::kMaxURLChars + 1, 'a');
    KURL input(NullURL(), url.c_str());
    KURL output;
    EXPECT_TRUE(remote->BounceUrl(input, &output));
    EXPECT_TRUE(output.IsEmpty());
  }

  // Test basic Origin serialization.
  scoped_refptr<const SecurityOrigin> non_unique =
      SecurityOrigin::CreateFromValidTuple("http", "www.google.com", 80);
  scoped_refptr<const SecurityOrigin> output;
  EXPECT_TRUE(remote->BounceOrigin(non_unique, &output));
  EXPECT_TRUE(non_unique->IsSameOriginWith(output.get()));
  EXPECT_FALSE(output->IsOpaque());

  scoped_refptr<const SecurityOrigin> unique =
      SecurityOrigin::CreateUniqueOpaque();
  EXPECT_TRUE(remote->BounceOrigin(unique, &output));
  EXPECT_TRUE(output->IsOpaque());
}

}  // namespace url
