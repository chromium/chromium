// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/compiler_specific.h"
#include "base/test/task_environment.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"

namespace ppapi {

namespace {

int mock_var_alive_count = 0;

class MockStringVar : public StringVar {
 public:
  MockStringVar(const std::string& str) : StringVar(str) {
    mock_var_alive_count++;
  }
  ~MockStringVar() override { mock_var_alive_count--; }
  bool HasValidVarID() { return GetExistingVarID() != 0; }
};

class MockObjectVar : public Var {
 public:
  MockObjectVar() : Var() { mock_var_alive_count++; }
  ~MockObjectVar() override { mock_var_alive_count--; }
  PP_VarType GetType() const override { return PP_VARTYPE_OBJECT; }
  bool HasValidVarID() { return GetExistingVarID() != 0; }
};

}  // namespace

class VarTrackerTest : public testing::Test {
 public:
  VarTrackerTest() {}

  // Test implementation.
  void SetUp() override {
    ASSERT_EQ(0, mock_var_alive_count);
  }
  void TearDown() override {}

  VarTracker& var_tracker() { return *globals_.GetVarTracker(); }

 private:
  base::test::SingleThreadTaskEnvironment
      task_environment_;  // Required to receive callbacks.
  TestGlobals globals_;
};

// Test that ResetVarID is called when the last PP_Var ref was deleted but the
// object lives on.
TEST_F(VarTrackerTest, LastResourceRef) {
  ProxyAutoLock lock;
  scoped_refptr<MockStringVar> var(new MockStringVar(std::string("xyz")));
  PP_Var pp_var = var->GetPPVar();
  EXPECT_TRUE(var->HasValidVarID());
  EXPECT_TRUE(var_tracker().GetVar(var->GetExistingVarID()));

  // Releasing it should keep the object (because we have a ref) but reset the
  // var_id_.
  EXPECT_TRUE(var_tracker().ReleaseVar(pp_var));
  EXPECT_FALSE(var->HasValidVarID());
  EXPECT_EQ(1, mock_var_alive_count);

  var.reset();
  EXPECT_EQ(0, mock_var_alive_count);
}

TEST_F(VarTrackerTest, GetPluginRefAgain) {
  ProxyAutoLock lock;
  scoped_refptr<MockStringVar> var(new MockStringVar(std::string("xyz")));
  PP_Var pp_var = var->GetPPVar();
  EXPECT_TRUE(var_tracker().ReleaseVar(pp_var));
  EXPECT_FALSE(var->HasValidVarID());
  EXPECT_EQ(1, mock_var_alive_count);

  // Obtaining PP_Var ref again, and add ref from VarTracker.
  pp_var = var->GetPPVar();
  EXPECT_TRUE(var->HasValidVarID());
  EXPECT_TRUE(var_tracker().GetVar(var->GetExistingVarID()));
  scoped_refptr<MockStringVar> another_var =
      static_cast<MockStringVar*>(var_tracker().GetVar(pp_var));
  EXPECT_EQ(1, mock_var_alive_count);

  // Releasing it again.
  EXPECT_TRUE(var_tracker().ReleaseVar(pp_var));
  EXPECT_FALSE(var->HasValidVarID());
  EXPECT_EQ(1, mock_var_alive_count);

  var.reset();
  EXPECT_FALSE(var_tracker().GetVar(pp_var));
  EXPECT_EQ(1, mock_var_alive_count);
  another_var.reset();
  EXPECT_FALSE(var_tracker().GetVar(pp_var));
  EXPECT_EQ(0, mock_var_alive_count);
}

// Tests when the plugin is holding a ref to a PP_Var when the instance is
// owned only by VarTracker.
TEST_F(VarTrackerTest, PluginRefWithoutVarRef) {
  ProxyAutoLock lock;
  // Make a PP_Var with one ref held by the plugin, and release the reference.
  scoped_refptr<MockStringVar> var(new MockStringVar(std::string("zzz")));
  PP_Var pp_var = var->GetPPVar();
  EXPECT_EQ(1, mock_var_alive_count);
  var.reset();
  EXPECT_EQ(1, mock_var_alive_count);

  // The var is owned only by VarTracker. PP_Var must be still valid.
  EXPECT_TRUE(var_tracker().GetVar(pp_var));

  var_tracker().ReleaseVar(pp_var);
  EXPECT_EQ(0, mock_var_alive_count);
  EXPECT_FALSE(var_tracker().GetVar(pp_var));
}

// Tests on Var having type of PP_VARTYPE_OBJECT.
TEST_F(VarTrackerTest, ObjectRef) {
  ProxyAutoLock lock;
  scoped_refptr<MockObjectVar> var(new MockObjectVar());
  PP_Var pp_var = var->GetPPVar();
  EXPECT_TRUE(var_tracker().ReleaseVar(pp_var));
  EXPECT_FALSE(var->HasValidVarID());
  EXPECT_EQ(1, mock_var_alive_count);

  // Obtaining PP_Var ref again, and add ref from VarTracker.
  pp_var = var->GetPPVar();
  EXPECT_TRUE(var->HasValidVarID());
  EXPECT_TRUE(var_tracker().GetVar(var->GetExistingVarID()));
  scoped_refptr<MockObjectVar> another_var =
      static_cast<MockObjectVar*>(var_tracker().GetVar(pp_var));
  EXPECT_EQ(1, mock_var_alive_count);

  // Releasing all references, then only VarTracker own the instance.
  var.reset();
  EXPECT_TRUE(var_tracker().GetVar(pp_var));
  EXPECT_EQ(1, mock_var_alive_count);
  another_var.reset();
  EXPECT_TRUE(var_tracker().GetVar(pp_var));
  EXPECT_EQ(1, mock_var_alive_count);

  // Releasing plugin reference.
  EXPECT_TRUE(var_tracker().ReleaseVar(pp_var));
  EXPECT_EQ(0, mock_var_alive_count);
}

}  // namespace ppapi
