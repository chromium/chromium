// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/callback_holder.h"

#include "base/functional/bind.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static void SetBool(bool* var) {
  DCHECK(!*var);
  *var = true;
}

static void CopyVar(int var1, int* var2) {
  DCHECK_NE(var1, *var2);
  *var2 = var1;
}

TEST(CallbackHolderTest, SetAfterHold_Closure) {
  CallbackHolder<base::OnceClosure> cb;
  EXPECT_TRUE(cb.IsNull());

  cb.HoldCallback();

  bool closure_called = false;
  cb.SetCallback(base::BindOnce(&SetBool, &closure_called));
  EXPECT_FALSE(cb.IsNull());

  cb.RunOrHold();
  EXPECT_FALSE(closure_called);

  EXPECT_FALSE(cb.IsNull());
  cb.RunHeldCallback();
  EXPECT_TRUE(cb.IsNull());
  EXPECT_TRUE(closure_called);
}

TEST(CallbackHolderTest, HoldAfterSet_Closure) {
  CallbackHolder<base::OnceClosure> cb;
  EXPECT_TRUE(cb.IsNull());

  bool closure_called = false;
  cb.SetCallback(base::BindOnce(&SetBool, &closure_called));
  EXPECT_FALSE(cb.IsNull());

  cb.HoldCallback();

  cb.RunOrHold();
  EXPECT_FALSE(closure_called);
  EXPECT_FALSE(cb.IsNull());
  cb.RunHeldCallback();
  EXPECT_TRUE(cb.IsNull());
  EXPECT_TRUE(closure_called);
}

TEST(CallbackHolderTest, NotHold_Closure) {
  CallbackHolder<base::OnceClosure> cb;
  EXPECT_TRUE(cb.IsNull());

  bool closure_called = false;
  cb.SetCallback(base::BindOnce(&SetBool, &closure_called));
  EXPECT_FALSE(cb.IsNull());

  cb.RunOrHold();
  EXPECT_TRUE(cb.IsNull());
  EXPECT_TRUE(closure_called);
}

TEST(CallbackHolderTest, SetAfterHold_Callback) {
  CallbackHolder<base::OnceCallback<void(int, int*)>> cb;
  EXPECT_TRUE(cb.IsNull());

  cb.HoldCallback();

  cb.SetCallback(base::BindOnce(&CopyVar));
  EXPECT_FALSE(cb.IsNull());

  int var1 = 100;
  int var2 = 0;
  cb.RunOrHold(var1, &var2);
  EXPECT_FALSE(cb.IsNull());
  EXPECT_NE(var1, var2);

  cb.RunHeldCallback();
  EXPECT_TRUE(cb.IsNull());
  EXPECT_EQ(var1, var2);
}

TEST(CallbackHolderTest, HoldAfterSet_Callback) {
  CallbackHolder<base::OnceCallback<void(int, int*)>> cb;
  EXPECT_TRUE(cb.IsNull());

  cb.SetCallback(base::BindOnce(&CopyVar));
  EXPECT_FALSE(cb.IsNull());

  cb.HoldCallback();

  int var1 = 100;
  int var2 = 0;
  cb.RunOrHold(var1, &var2);
  EXPECT_FALSE(cb.IsNull());
  EXPECT_NE(var1, var2);

  cb.RunHeldCallback();
  EXPECT_TRUE(cb.IsNull());
  EXPECT_EQ(var1, var2);
}

TEST(CallbackHolderTest, NotHold_Callback) {
  CallbackHolder<base::OnceCallback<void(int, int*)>> cb;
  EXPECT_TRUE(cb.IsNull());

  cb.SetCallback(base::BindOnce(&CopyVar));
  EXPECT_FALSE(cb.IsNull());

  int var1 = 100;
  int var2 = 0;
  cb.RunOrHold(var1, &var2);
  EXPECT_TRUE(cb.IsNull());
  EXPECT_EQ(var1, var2);
}

}  // namespace media
