// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/threading/platform_thread.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/proxy/ppapi_proxy_test.h"
#include "ppapi/shared_impl/ppb_var_shared.h"

namespace {
std::string VarToString(const PP_Var& var, const PPB_Var* ppb_var) {
  uint32_t len = 0;
  const char* utf8 = ppb_var->VarToUtf8(var, &len);
  return std::string(utf8, len);
}
const size_t kNumStrings = 100;
const size_t kNumThreads = 20;
const int kRefsToAdd = 20;
}  // namespace

namespace ppapi {
namespace proxy {

class PPB_VarTest : public PluginProxyTest {
 public:
  PPB_VarTest()
      : test_strings_(kNumStrings), vars_(kNumStrings),
        ppb_var_(ppapi::PPB_Var_Shared::GetVarInterface1_2()) {
    // Set the value of test_strings_[i] to "i".
    for (size_t i = 0; i < kNumStrings; ++i)
      test_strings_[i] = base::NumberToString(i);
  }
 protected:
  std::vector<std::string> test_strings_;
  std::vector<PP_Var> vars_;
  const PPB_Var* ppb_var_;
};

// Test basic String operations.
TEST_F(PPB_VarTest, Strings) {
  for (size_t i = 0; i < kNumStrings; ++i) {
    vars_[i] = ppb_var_->VarFromUtf8(
        test_strings_[i].c_str(),
        static_cast<uint32_t>(test_strings_[i].length()));
    EXPECT_EQ(test_strings_[i], VarToString(vars_[i], ppb_var_));
  }
  // At this point, they should each have a ref count of 1. Add some more.
  for (int ref = 0; ref < kRefsToAdd; ++ref) {
    for (size_t i = 0; i < kNumStrings; ++i) {
      ppb_var_->AddRef(vars_[i]);
      // Make sure the string is still there with the right value.
      EXPECT_EQ(test_strings_[i], VarToString(vars_[i], ppb_var_));
    }
  }
  for (int ref = 0; ref < kRefsToAdd; ++ref) {
    for (size_t i = 0; i < kNumStrings; ++i) {
      ppb_var_->Release(vars_[i]);
      // Make sure the string is still there with the right value.
      EXPECT_EQ(test_strings_[i], VarToString(vars_[i], ppb_var_));
    }
  }
  // Now remove the ref counts for each string and make sure they are gone.
  for (size_t i = 0; i < kNumStrings; ++i) {
    ppb_var_->Release(vars_[i]);
    uint32_t len = 10;
    const char* utf8 = ppb_var_->VarToUtf8(vars_[i], &len);
    EXPECT_EQ(NULL, utf8);
    EXPECT_EQ(0u, len);
  }
}

// PPB_VarTest.Threads tests string operations accessed by multiple threads.
namespace {
// These three delegate classes which precede the test are for use with
// PlatformThread. The test goes roughly like this:
// 1) Spawn kNumThreads 'CreateVar' threads, giving each a roughly equal subset
//    of test_strings_ to 'create'. Each 'CreateVar' thread also converts its
//    set of vars back in to strings so that the main test thread can verify
//    their values were correctly converted.
// 2) Spawn kNumThreads 'ChangeRefVar' threads. Each of these threads will
//    incremement & decrement the reference count of ALL vars kRefsToAdd times.
//    Finally, each thread adds 1 ref count. This leaves each var with a ref-
//    count of |kNumThreads + 1|. The main test thread removes a ref, leaving
//    each var with a ref-count of |kNumThreads|.
// 3) Spawn kNumThreads 'RemoveVar' threads. Each of these threads releases each
//    var once. Once all the threads have finished, there should be no vars
//    left.
class CreateVarThreadDelegate : public base::PlatformThread::Delegate {
 public:
  // |strings_in|, |vars|, and |strings_out| are arrays, and size is their size.
  // For each |strings_in[i]|, we will set |vars[i]| using that value. Then we
  // read the var back out to |strings_out[i]|.
  CreateVarThreadDelegate(const std::string* strings_in,
                          PP_Var* vars_out,
                          std::string* strings_out,
                          size_t size)
      : strings_in_(strings_in),
        vars_out_(vars_out),
        strings_out_(strings_out),
        size_(size) {}
  virtual ~CreateVarThreadDelegate() {}
  virtual void ThreadMain() {
    const PPB_Var* ppb_var = ppapi::PPB_Var_Shared::GetVarInterface1_2();
    for (size_t i = 0; i < size_; ++i) {
      vars_out_[i] = ppb_var->VarFromUtf8(
          strings_in_[i].c_str(),
          static_cast<uint32_t>(strings_in_[i].length()));
      strings_out_[i] = VarToString(vars_out_[i], ppb_var);
    }
  }
 private:
  const std::string* strings_in_;
  PP_Var* vars_out_;
  std::string* strings_out_;
  size_t size_;
};

// A thread that will increment and decrement the reference count of every var
// multiple times.
class ChangeRefVarThreadDelegate : public base::PlatformThread::Delegate {
 public:
  ChangeRefVarThreadDelegate(const std::vector<PP_Var>& vars) : vars_(vars) {
  }
  virtual ~ChangeRefVarThreadDelegate() {}
  virtual void ThreadMain() {
    const PPB_Var* ppb_var = ppapi::PPB_Var_Shared::GetVarInterface1_2();
    // Increment and decrement the reference count for each var kRefsToAdd
    // times. Note that we always AddRef once before doing the matching Release,
    // to ensure that we never accidentally release the last reference.
    for (int ref = 0; ref < kRefsToAdd; ++ref) {
      for (size_t i = 0; i < kNumStrings; ++i) {
        ppb_var->AddRef(vars_[i]);
        ppb_var->Release(vars_[i]);
      }
    }
    // Now add 1 ref to each Var. The net result is that all Vars will have a
    // ref-count of (kNumThreads + 1) after this. That will allow us to have all
    // threads release all vars later.
    for (size_t i = 0; i < kNumStrings; ++i) {
      ppb_var->AddRef(vars_[i]);
    }
  }
 private:
  std::vector<PP_Var> vars_;
};

// A thread that will decrement the reference count of every var once.
class RemoveRefVarThreadDelegate : public base::PlatformThread::Delegate {
 public:
  RemoveRefVarThreadDelegate(const std::vector<PP_Var>& vars) : vars_(vars) {
  }
  virtual ~RemoveRefVarThreadDelegate() {}
  virtual void ThreadMain() {
    const PPB_Var* ppb_var = ppapi::PPB_Var_Shared::GetVarInterface1_2();
    for (size_t i = 0; i < kNumStrings; ++i) {
      ppb_var->Release(vars_[i]);
    }
  }
 private:
  std::vector<PP_Var> vars_;
};

}  // namespace

TEST_F(PPB_VarTest, Threads) {
  std::vector<base::PlatformThreadHandle> create_var_threads(kNumThreads);
  std::vector<CreateVarThreadDelegate> create_var_delegates;
  // The strings that the threads will re-extract from Vars (so we can check
  // that they match the original strings).
  std::vector<std::string> strings_out(kNumStrings);
  size_t strings_per_thread = kNumStrings/kNumThreads;
  // Give each thread an equal slice of strings to turn in to vars. (Except the
  // last thread may get fewer if kNumStrings is not evenly divisible by
  // kNumThreads).
  for (size_t slice_start= 0; slice_start < kNumStrings;
       slice_start += strings_per_thread) {
    create_var_delegates.push_back(CreateVarThreadDelegate(
        &test_strings_[slice_start], &vars_[slice_start],
        &strings_out[slice_start],
        std::min(strings_per_thread, kNumStrings - slice_start)));
  }
  // Now run then join all the threads.
  for (size_t i = 0; i < kNumThreads; ++i)
    base::PlatformThread::Create(0, &create_var_delegates[i],
                                 &create_var_threads[i]);
  for (size_t i = 0; i < kNumThreads; ++i)
    base::PlatformThread::Join(create_var_threads[i]);
  // Now check that the strings have the expected values.
  EXPECT_EQ(test_strings_, strings_out);

  // Tinker with the reference counts in a multithreaded way.
  std::vector<base::PlatformThreadHandle> change_ref_var_threads(kNumThreads);
  std::vector<ChangeRefVarThreadDelegate> change_ref_var_delegates;
  for (size_t i = 0; i < kNumThreads; ++i)
    change_ref_var_delegates.push_back(ChangeRefVarThreadDelegate(vars_));
  for (size_t i = 0; i < kNumThreads; ++i) {
    base::PlatformThread::Create(0, &change_ref_var_delegates[i],
                                 &change_ref_var_threads[i]);
  }
  for (size_t i = 0; i < kNumThreads; ++i)
    base::PlatformThread::Join(change_ref_var_threads[i]);

  // Now each var has a refcount of (kNumThreads + 1). Let's decrement each var
  // once so that every 'RemoveRef' thread (spawned below) owns 1 reference, and
  // when the last one removes a ref, the Var will be deleted.
  for (size_t i = 0; i < kNumStrings; ++i) {
    ppb_var_->Release(vars_[i]);
  }

  // Check that all vars are still valid and have the values we expect.
  for (size_t i = 0; i < kNumStrings; ++i)
    EXPECT_EQ(test_strings_[i], VarToString(vars_[i], ppb_var_));

  // Remove the last reference counts for all vars.
  std::vector<base::PlatformThreadHandle> remove_ref_var_threads(kNumThreads);
  std::vector<RemoveRefVarThreadDelegate> remove_ref_var_delegates;
  for (size_t i = 0; i < kNumThreads; ++i)
    remove_ref_var_delegates.push_back(RemoveRefVarThreadDelegate(vars_));
  for (size_t i = 0; i < kNumThreads; ++i) {
    base::PlatformThread::Create(0, &remove_ref_var_delegates[i],
                                 &remove_ref_var_threads[i]);
  }
  for (size_t i = 0; i < kNumThreads; ++i)
    base::PlatformThread::Join(remove_ref_var_threads[i]);

  // All the vars should no longer represent valid strings.
  for (size_t i = 0; i < kNumStrings; ++i) {
    uint32_t len = 10;
    const char* utf8 = ppb_var_->VarToUtf8(vars_[i], &len);
    EXPECT_EQ(NULL, utf8);
    EXPECT_EQ(0u, len);
  }
}

}  // namespace proxy
}  // namespace ppapi
