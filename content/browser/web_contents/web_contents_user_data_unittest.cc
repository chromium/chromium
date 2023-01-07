// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/web_contents_user_data.h"

#include <memory>

#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

class WebContentsAttachedClass1
    : public WebContentsUserData<WebContentsAttachedClass1> {
 public:
  ~WebContentsAttachedClass1() override = default;

 private:
  explicit WebContentsAttachedClass1(WebContents* contents)
      : WebContentsUserData<WebContentsAttachedClass1>(*contents) {}
  friend class WebContentsUserData<WebContentsAttachedClass1>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsAttachedClass1);

class WebContentsAttachedClass2
    : public WebContentsUserData<WebContentsAttachedClass2> {
 public:
  ~WebContentsAttachedClass2() override = default;

 private:
  explicit WebContentsAttachedClass2(WebContents* contents)
      : WebContentsUserData<WebContentsAttachedClass2>(*contents) {}
  friend class WebContentsUserData<WebContentsAttachedClass2>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WebContentsAttachedClass2);

typedef RenderViewHostTestHarness WebContentsUserDataTest;

TEST_F(WebContentsUserDataTest, OneInstanceTwoAttachments) {
  WebContents* contents = web_contents();
  WebContentsAttachedClass1* class1 =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_EQ(nullptr, class1);
  WebContentsAttachedClass2* class2 =
      WebContentsAttachedClass2::FromWebContents(contents);
  ASSERT_EQ(nullptr, class2);

  WebContentsAttachedClass1::CreateForWebContents(contents);
  class1 = WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(class1 != nullptr);
  class2 = WebContentsAttachedClass2::FromWebContents(contents);
  ASSERT_EQ(nullptr, class2);

  WebContentsAttachedClass2::CreateForWebContents(contents);
  WebContentsAttachedClass1* class1again =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(class1again != nullptr);
  class2 = WebContentsAttachedClass2::FromWebContents(contents);
  ASSERT_TRUE(class2 != nullptr);
  ASSERT_EQ(class1, class1again);
  ASSERT_NE(static_cast<void*>(class1), static_cast<void*>(class2));
}

TEST_F(WebContentsUserDataTest, TwoInstancesOneAttachment) {
  WebContents* contents1 = web_contents();
  std::unique_ptr<WebContents> contents2(
      WebContentsTester::CreateTestWebContents(browser_context(), nullptr));

  WebContentsAttachedClass1* one_class =
      WebContentsAttachedClass1::FromWebContents(contents1);
  ASSERT_EQ(nullptr, one_class);
  WebContentsAttachedClass1* two_class =
      WebContentsAttachedClass1::FromWebContents(contents2.get());
  ASSERT_EQ(nullptr, two_class);

  WebContentsAttachedClass1::CreateForWebContents(contents1);
  one_class = WebContentsAttachedClass1::FromWebContents(contents1);
  ASSERT_TRUE(one_class != nullptr);
  two_class = WebContentsAttachedClass1::FromWebContents(contents2.get());
  ASSERT_EQ(nullptr, two_class);

  WebContentsAttachedClass1::CreateForWebContents(contents2.get());
  WebContentsAttachedClass1* one_class_again =
      WebContentsAttachedClass1::FromWebContents(contents1);
  ASSERT_TRUE(one_class_again != nullptr);
  two_class = WebContentsAttachedClass1::FromWebContents(contents2.get());
  ASSERT_TRUE(two_class != nullptr);
  ASSERT_EQ(one_class, one_class_again);
  ASSERT_NE(one_class, two_class);
}

TEST_F(WebContentsUserDataTest, Idempotence) {
  WebContents* contents = web_contents();
  WebContentsAttachedClass1* clazz =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_EQ(nullptr, clazz);

  WebContentsAttachedClass1::CreateForWebContents(contents);
  clazz = WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(clazz != nullptr);

  WebContentsAttachedClass1::CreateForWebContents(contents);
  WebContentsAttachedClass1* class_again =
      WebContentsAttachedClass1::FromWebContents(contents);
  ASSERT_TRUE(class_again != nullptr);
  ASSERT_EQ(clazz, class_again);
}

class NonCopyableNonMovableClass {
 public:
  explicit NonCopyableNonMovableClass(int value) : value_(value) {}

  NonCopyableNonMovableClass(const NonCopyableNonMovableClass&) = delete;
  NonCopyableNonMovableClass& operator=(const NonCopyableNonMovableClass&) =
      delete;
  NonCopyableNonMovableClass(NonCopyableNonMovableClass&&) = delete;
  NonCopyableNonMovableClass& operator=(NonCopyableNonMovableClass&&) = delete;

  int value() const { return value_; }

 private:
  int value_;
};

class AttachedClassWithParams
    : public WebContentsUserData<AttachedClassWithParams> {
 public:
  ~AttachedClassWithParams() override = default;

  int param1() const { return param1_; }
  int param2() const { return param2_; }

 private:
  friend class WebContentsUserData<AttachedClassWithParams>;

  explicit AttachedClassWithParams(WebContents* contents,
                                   int param1,
                                   NonCopyableNonMovableClass&& param2)
      : WebContentsUserData<AttachedClassWithParams>(*contents),
        param1_(param1),
        param2_(param2.value()) {}

  int param1_;
  int param2_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(AttachedClassWithParams);

TEST_F(WebContentsUserDataTest, CreateWithParameters) {
  ASSERT_EQ(nullptr, AttachedClassWithParams::FromWebContents(web_contents()));

  AttachedClassWithParams::CreateForWebContents(web_contents(), 1,
                                                NonCopyableNonMovableClass(42));
  auto* instance = AttachedClassWithParams::FromWebContents(web_contents());
  ASSERT_NE(nullptr, instance);

  EXPECT_EQ(1, instance->param1());
  EXPECT_EQ(42, instance->param2());
}

}  // namespace content
